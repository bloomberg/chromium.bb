// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>

#include <array>
#include <chrono>  // NOLINT

#include "cast/standalone_receiver/cast_agent.h"
#include "cast/streaming/ssrc.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/logging.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "util/trace_logging.h"

namespace openscreen {
namespace cast {
namespace {

void RunStandaloneReceiver(TaskRunnerImpl* task_runner,
                           InterfaceInfo interface) {
  CastAgent agent(task_runner, interface);
  agent.Start();

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
    usage: argv[0] <options>

      -i, --interface= <interface, e.g. wlan0, eth0>
           Specify the network interface to bind to. The interface is looked
           up from the system interface registry. This argument is optional, and
           omitting it causes the standalone receiver to attempt to bind to
           ANY (0.0.0.0) on default port 2344. Note that this mode does not
           work reliably on some platforms.

      -t, --tracing: Enable performance tracing logging.

      -h, --help: Show this help message.
  )";
  std::string message = kUsageMessage;
  message.replace(message.find(kExecutableTag), strlen(kExecutableTag), argv0);
  OSP_LOG_ERROR << message;
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
      {"interface", required_argument, nullptr, 'i'},
      {"tracing", no_argument, nullptr, 't'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  InterfaceInfo interface_info;
  std::unique_ptr<openscreen::TextTraceLoggingPlatform> trace_logger;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "i:th", argument_options, nullptr)) !=
         -1) {
    switch (ch) {
      case 'i': {
        std::vector<InterfaceInfo> network_interfaces =
            openscreen::GetNetworkInterfaces();
        for (auto& interface : network_interfaces) {
          if (interface.name == optarg) {
            interface_info = std::move(interface);
          }
        }

        if (interface_info.name.empty()) {
          OSP_LOG_ERROR << "Invalid interface specified: " << optarg;
          return 1;
        }
        break;
      }
      case 't':
        trace_logger = std::make_unique<openscreen::TextTraceLoggingPlatform>();
        break;
      case 'h':
        LogUsage(argv[0]);
        return 1;
    }
  }

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50},
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  // Runs until the process is interrupted.  Safe to pass |task_runner| as it
  // will not be destroyed by ShutDown() until this exits.
  openscreen::cast::RunStandaloneReceiver(task_runner, interface_info);
  PlatformClientPosix::ShutDown();
  return 0;
}
