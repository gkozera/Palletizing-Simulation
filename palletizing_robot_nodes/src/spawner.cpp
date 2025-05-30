#include <rclcpp/rclcpp.hpp>
#include <gazebo_msgs/srv/spawn_entity.hpp>
#include <palletizing_robot_interfaces/srv/palletization.hpp> 
#include <moveit/planning_scene_interface/planning_scene_interface.h>
#include <geometry_msgs/msg/pose.hpp>
#include <chrono>
#include <thread>
#include "collision_environment.hpp"
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <fstream>
#include <sstream>

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    auto const node = std::make_shared<rclcpp::Node>(
        "spawner_node",
        rclcpp::NodeOptions().automatically_declare_parameters_from_overrides(true)
      );
    auto const logger = rclcpp::get_logger("spawner_node");

    moveit::planning_interface::PlanningSceneInterface planning_scene_interface;
    collision_environment::generateCollisionEnviroment(planning_scene_interface);

    // Creating client
    auto client = node->create_client<gazebo_msgs::srv::SpawnEntity>("/spawn_entity");
    auto palletization_client = node->create_client<palletizing_robot_interfaces::srv::Palletization>("/Palletization");
    while (!client->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_WARN(node->get_logger(), "Waiting for service /spawn_entity...");
    }
    while (!palletization_client->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_WARN(node->get_logger(), "Waiting for service /Palletization...");
    }


    std::string model_path = ament_index_cpp::get_package_share_directory("palletizing_robot_gazebo") + "/" + "models/small_box/model.sdf";
    RCLCPP_INFO(logger, "Model path: %s", model_path.c_str());
    std::ifstream file(model_path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    geometry_msgs::msg::Pose pose;
    pose.position.x = 1.0;
    pose.position.y = 0.0;
    pose.position.z = 0.55;

    auto request = std::make_shared<gazebo_msgs::srv::SpawnEntity::Request>();
    auto palletization_request = std::make_shared<palletizing_robot_interfaces::srv::Palletization::Request>();
    request->xml = buffer.str();
    request->initial_pose = pose;
    
    for(int i=0; i<48; i++){
        std::string box_id = "Box_" + std::to_string(i + 1);
        collision_environment::addMoveitBox(planning_scene_interface, box_id, pose, {0.18, 0.28, 0.1});

        // pose.position.x += 0.4;
        auto future = client->async_send_request(request);
        if (rclcpp::spin_until_future_complete(node, future) == rclcpp::FutureReturnCode::SUCCESS) {
            RCLCPP_INFO(logger, "Object spawned");
        } else {
            RCLCPP_ERROR(logger, "Faild to spawn object");
        }

        palletization_request->counter = i;
        if(i==0){ palletization_request->gazebo_name = "small_box"; }
        else{ palletization_request->gazebo_name = "small_box_" + std::to_string(i - 1); }
        palletization_request->moveit_name = box_id;

        auto palletization_future = palletization_client->async_send_request(palletization_request);
        if (rclcpp::spin_until_future_complete(node, palletization_future, std::chrono::seconds(40)) == rclcpp::FutureReturnCode::SUCCESS) {
            RCLCPP_INFO(logger, "Pick place operation succesful");
        } else {
            RCLCPP_ERROR(logger, "Faild pick place operation");
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } 

    //collision_environment::addMoveitBox(planning_scene_interface, "Box_1", {1, 0, 0.65}, {0.2, 0.3, 0.1});

    rclcpp::shutdown();
    return 0;
}