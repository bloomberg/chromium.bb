// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>

#include <array>
#include <chrono>  // NOLINT

#include "cast/common/public/service_info.h"
#include "cast/standalone_receiver/cast_agent.h"
#include "cast/streaming/ssrc.h"
#include "discovery/common/config.h"
#include "discovery/common/reporting_client.h"
#include "discovery/public/dns_sd_service_factory.h"
#include "discovery/public/dns_sd_service_publisher.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/logging.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "util/stringprintf.h"
#include "util/trace_logging.h"

namespace openscreen {
namespace cast {
namespace {

class DiscoveryReportingClient : public discovery::ReportingClient {
  void OnFatalError(Error error) override {
    OSP_LOG_FATAL << "Encountered fatal discovery error: " << error;
  }

  void OnRecoverableError(Error error) override {
    OSP_LOG_ERROR << "Encountered recoverable discovery error: " << error;
  }
};

struct DiscoveryState {
  SerialDeletePtr<discovery::DnsSdService> service;
  std::unique_ptr<DiscoveryReportingClient> reporting_client;
  std::unique_ptr<discovery::DnsSdServicePublisher<ServiceInfo>> publisher;
};

ErrorOr<std::unique_ptr<DiscoveryState>> StartDiscovery(
    TaskRunner* task_runner,
    const InterfaceInfo& interface) {
  discovery::Config config;

  config.interface = interface;

  auto state = std::make_unique<DiscoveryState>();
  state->reporting_client = std::make_unique<DiscoveryReportingClient>();
  state->service = discovery::CreateDnsSdService(
      task_runner, state->reporting_client.get(), config);

  // TODO(jophba): update after ServiceInfo update patch lands.
  ServiceInfo info;
  info.port = kDefaultCastPort;
  if (interface.GetIpAddressV4()) {
    info.v4_address = interface.GetIpAddressV4();
  }
  if (interface.GetIpAddressV6()) {
    info.v6_address = interface.GetIpAddressV6();
  }

  OSP_CHECK(std::any_of(interface.hardware_address.begin(),
                        interface.hardware_address.end(),
                        [](int e) { return e > 0; }));
  info.unique_id = HexEncode(interface.hardware_address);

  // TODO(jophba): add command line arguments to set these fields.
  info.model_name = "cast_standalone_receiver";
  info.friendly_name = "Cast Standalone Receiver";

  state->publisher =
      std::make_unique<discovery::DnsSdServicePublisher<ServiceInfo>>(
          state->service.get(), kCastV2ServiceId, ServiceInfoToDnsSdRecord);

  auto error = state->publisher->Register(info);
  if (!error.ok()) {
    return error;
  }
  return state;
}

void RunStandaloneReceiver(TaskRunnerImpl* task_runner,
                           InterfaceInfo interface) {
  CastAgent agent(task_runner, interface);
  const auto error = agent.Start();
  if (!error.ok()) {
    OSP_LOG_ERROR << "Error occurred while starting agent: " << error;
    return;
  }

  // Run the event loop until an exit is requested (e.g., the video player GUI
  // window is closed, a SIGINT or SIGTERM is received, or whatever other
  // appropriate user indication that shutdown is requested).
  task_runner->RunUntilSignaled();
}

}  // namespace
}  // namespace cast
}  // namespace openscreen

namespace {

void LogUsage(const char* argv0) {
  constexpr char kExecutableTag[] = "argv[0]";
  constexpr char kUsageMessage[] = R"(
    usage: argv[0] <options> <interface>

    options:
      <interface>: Specify the network interface to bind to. The interface is
          looked up from the system interface registry. This argument is
          mandatory, as it must be known for publishing discovery.

      -t, --tracing: Enable performance tracing logging.

      -h, --help: Show this help message.
  )";
  std::string message = kUsageMessage;
  message.replace(message.find(kExecutableTag), strlen(kExecutableTag), argv0);
  OSP_LOG_INFO << message;
}

}  // namespace

int main(int argc, char* argv[]) {
  // TODO(jophba): refactor into separate method and make main a one-liner.
  using openscreen::Clock;
  using openscreen::ErrorOr;
  using openscreen::InterfaceInfo;
  using openscreen::IPAddress;
  using openscreen::IPEndpoint;
  using openscreen::PlatformClientPosix;
  using openscreen::TaskRunnerImpl;

  openscreen::SetLogLevel(openscreen::LogLevel::kInfo);

  const struct option argument_options[] = {
      {"tracing", no_argument, nullptr, 't'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  InterfaceInfo interface_info;
  std::unique_ptr<openscreen::TextTraceLoggingPlatform> trace_logger;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "th", argument_options, nullptr)) !=
         -1) {
    switch (ch) {
      case 't':
        trace_logger = std::make_unique<openscreen::TextTraceLoggingPlatform>();
        break;
      case 'h':
        LogUsage(argv[0]);
        return 1;
    }
  }
  char* interface_argument = argv[optind];
  OSP_CHECK(interface_argument != nullptr)
      << "Missing mandatory argument: interface.";
  std::vector<InterfaceInfo> network_interfaces =
      openscreen::GetNetworkInterfaces();
  for (auto& interface : network_interfaces) {
    if (interface.name == interface_argument) {
      interface_info = std::move(interface);
      break;
    }
  }
  OSP_CHECK(!interface_info.name.empty()) << "Invalid interface specified.";

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50},
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  auto discovery_state =
      openscreen::cast::StartDiscovery(task_runner, interface_info);
  OSP_CHECK(discovery_state.is_value()) << "Failed to start discovery.";

  // Runs until the process is interrupted.  Safe to pass |task_runner| as it
  // will not be destroyed by ShutDown() until this exits.
  openscreen::cast::RunStandaloneReceiver(task_runner, interface_info);

  // The task runner must be deleted after all serial delete pointers, such
  // as the one stored in the discovery state.
  discovery_state.value().reset();
  PlatformClientPosix::ShutDown();
  return 0;
}
