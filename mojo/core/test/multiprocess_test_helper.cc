// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/test/multiprocess_test_helper.h"

#include <functional>
#include <set>
#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/features.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "mojo/core/embedder/default_mach_broker.h"
#endif

#if !defined(OS_FUCHSIA)
#include "mojo/public/cpp/platform/named_platform_channel.h"
#endif

namespace mojo {
namespace core {
namespace test {

namespace {

#if !defined(OS_FUCHSIA)
const char kNamedPipeName[] = "named-pipe-name";
#endif
const char kRunAsBrokerClient[] = "run-as-broker-client";
const char kTestChildMessagePipeName[] = "test_pipe";

// For use (and only valid) in a test child process:
base::LazyInstance<IsolatedConnection>::Leaky g_child_isolated_connection;

template <typename Func>
int RunClientFunction(Func handler, bool pass_pipe_ownership_to_main) {
  CHECK(MultiprocessTestHelper::primordial_pipe.is_valid());
  ScopedMessagePipeHandle pipe =
      std::move(MultiprocessTestHelper::primordial_pipe);
  MessagePipeHandle pipe_handle =
      pass_pipe_ownership_to_main ? pipe.release() : pipe.get();
  return handler(pipe_handle.value());
}

}  // namespace

MultiprocessTestHelper::MultiprocessTestHelper() {}

MultiprocessTestHelper::~MultiprocessTestHelper() {
  CHECK(!test_child_.IsValid());
}

ScopedMessagePipeHandle MultiprocessTestHelper::StartChild(
    const std::string& test_child_name,
    LaunchType launch_type) {
  return StartChildWithExtraSwitch(test_child_name, std::string(),
                                   std::string(), launch_type);
}

ScopedMessagePipeHandle MultiprocessTestHelper::StartChildWithExtraSwitch(
    const std::string& test_child_name,
    const std::string& switch_string,
    const std::string& switch_value,
    LaunchType launch_type) {
  CHECK(!test_child_name.empty());
  CHECK(!test_child_.IsValid());

  std::string test_child_main = test_child_name + "TestChildMain";

  // Manually construct the new child's commandline to avoid copying unwanted
  // values.
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine().GetProgram());

  std::set<std::string> uninherited_args;
  uninherited_args.insert("mojo-platform-channel-handle");
  uninherited_args.insert(switches::kTestChildProcess);

  // Copy commandline switches from the parent process, except for the
  // multiprocess client name and mojo message pipe handle; this allows test
  // clients to spawn other test clients.
  for (const auto& entry :
       base::CommandLine::ForCurrentProcess()->GetSwitches()) {
    if (uninherited_args.find(entry.first) == uninherited_args.end())
      command_line.AppendSwitchNative(entry.first, entry.second);
  }

#if !defined(OS_FUCHSIA)
  NamedPlatformChannel::ServerName server_name;
#endif
  PlatformChannel channel;
  base::LaunchOptions options;
  switch (launch_type) {
    case LaunchType::CHILD:
    case LaunchType::PEER:
      channel.PrepareToPassRemoteEndpoint(&options, &command_line);
      break;
#if !defined(OS_FUCHSIA)
    case LaunchType::NAMED_CHILD:
    case LaunchType::NAMED_PEER: {
#if defined(OS_POSIX)
#if defined(OS_MACOSX)
      if (base::FeatureList::IsEnabled(features::kMojoChannelMac)) {
        server_name = NamedPlatformChannel::ServerNameFromUTF8(
            "mojo.test." + base::NumberToString(base::RandUint64()));
      } else {
#endif  // defined(OS_MACOSX)
        base::FilePath temp_dir;
        CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
        server_name =
            temp_dir.AppendASCII(base::NumberToString(base::RandUint64()))
                .value();
#if defined(OS_MACOSX)
      }
#endif  // defined(OS_MACOSX)
#elif defined(OS_WIN)
      server_name = base::NumberToString16(base::RandUint64());
#else
#error "Platform not yet supported."
#endif
      command_line.AppendSwitchNative(kNamedPipeName, server_name);
      break;
    }
#endif  // !defined(OS_FUCHSIA)
  }

  if (!switch_string.empty()) {
    CHECK(!command_line.HasSwitch(switch_string));
    if (!switch_value.empty())
      command_line.AppendSwitchASCII(switch_string, switch_value);
    else
      command_line.AppendSwitch(switch_string);
  }

#if defined(OS_WIN)
  options.start_hidden = true;
#endif

  // NOTE: In the case of named pipes, it's important that the server handle be
  // created before the child process is launched; otherwise the server binding
  // the pipe path can race with child's connection to the pipe.
  PlatformChannelEndpoint local_channel_endpoint;
  PlatformChannelServerEndpoint server_endpoint;
  switch (launch_type) {
    case LaunchType::CHILD:
    case LaunchType::PEER:
      local_channel_endpoint = channel.TakeLocalEndpoint();
      break;
#if !defined(OS_FUCHSIA)
    case LaunchType::NAMED_CHILD:
    case LaunchType::NAMED_PEER: {
      NamedPlatformChannel::Options options;
      options.server_name = server_name;
      NamedPlatformChannel named_channel(options);
      server_endpoint = named_channel.TakeServerEndpoint();
      break;
    }
#endif  // !defined(OS_FUCHSIA)
  };

  OutgoingInvitation child_invitation;
  ScopedMessagePipeHandle pipe;
  switch (launch_type) {
    case LaunchType::CHILD:
#if !defined(OS_FUCHSIA)
    case LaunchType::NAMED_CHILD:
#endif
      pipe = child_invitation.AttachMessagePipe(kTestChildMessagePipeName);
      command_line.AppendSwitch(kRunAsBrokerClient);
      break;
    case LaunchType::PEER:
#if !defined(OS_FUCHSIA)
    case LaunchType::NAMED_PEER:
#endif
      isolated_connection_ = std::make_unique<IsolatedConnection>();
      if (local_channel_endpoint.is_valid()) {
        pipe = isolated_connection_->Connect(std::move(local_channel_endpoint));
      } else {
#if defined(OS_POSIX) || defined(OS_WIN)
        DCHECK(server_endpoint.is_valid());
        pipe = isolated_connection_->Connect(std::move(server_endpoint));
#else
        NOTREACHED();
#endif
      }
      break;
  }

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // This lock needs to be held while launching the child because the Mach port
  // broker only allows task ports to be received from known child processes.
  // However, it can only know the child process's pid after the child has
  // launched. To prevent a race where the child process sends its task port
  // before the pid has been registered, the lock needs to be held over both
  // launch and child pid registration.
  auto* mach_broker = mojo::core::DefaultMachBroker::Get();
  mach_broker->GetLock().Acquire();
#endif

  test_child_ =
      base::SpawnMultiProcessTestChild(test_child_main, command_line, options);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (test_child_.IsValid())
    mach_broker->ExpectPid(test_child_.Pid());
  mach_broker->GetLock().Release();
#endif

  if (launch_type == LaunchType::CHILD || launch_type == LaunchType::PEER)
    channel.RemoteProcessLaunchAttempted();

  if (launch_type == LaunchType::CHILD) {
    DCHECK(local_channel_endpoint.is_valid());
    OutgoingInvitation::Send(std::move(child_invitation), test_child_.Handle(),
                             std::move(local_channel_endpoint),
                             ProcessErrorCallback());
  }
#if !defined(OS_FUCHSIA)
  else if (launch_type == LaunchType::NAMED_CHILD) {
    DCHECK(server_endpoint.is_valid());
    OutgoingInvitation::Send(std::move(child_invitation), test_child_.Handle(),
                             std::move(server_endpoint),
                             ProcessErrorCallback());
  }
#endif  //  !defined(OS_FUCHSIA)

  CHECK(test_child_.IsValid());
  return pipe;
}

int MultiprocessTestHelper::WaitForChildShutdown() {
  CHECK(test_child_.IsValid());

  int rv = -1;
  WaitForMultiprocessTestChildExit(test_child_, TestTimeouts::action_timeout(),
                                   &rv);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  auto* mach_broker = mojo::core::DefaultMachBroker::Get();
  base::AutoLock lock(mach_broker->GetLock());
  mach_broker->RemovePid(test_child_.Pid());
#endif

  test_child_.Close();
  return rv;
}

bool MultiprocessTestHelper::WaitForChildTestShutdown() {
  return WaitForChildShutdown() == 0;
}

// static
void MultiprocessTestHelper::ChildSetup() {
  CHECK(base::CommandLine::InitializedForCurrentProcess());

  auto& command_line = *base::CommandLine::ForCurrentProcess();

  bool run_as_broker_client = command_line.HasSwitch(kRunAsBrokerClient);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (run_as_broker_client)
    DefaultMachBroker::SendTaskPortToParent();
#endif

  PlatformChannelEndpoint endpoint;
#if !defined(OS_FUCHSIA)
  NamedPlatformChannel::ServerName named_pipe(
      command_line.GetSwitchValueNative(kNamedPipeName));
  if (!named_pipe.empty()) {
    endpoint = NamedPlatformChannel::ConnectToServer(named_pipe);
  } else
#endif  // !defined(OS_FUCHSIA)
  {
    endpoint =
        PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line);
  }

  if (run_as_broker_client) {
    IncomingInvitation invitation =
        IncomingInvitation::Accept(std::move(endpoint));
    primordial_pipe = invitation.ExtractMessagePipe(kTestChildMessagePipeName);
  } else {
    primordial_pipe =
        g_child_isolated_connection.Get().Connect(std::move(endpoint));
  }
}

// static
int MultiprocessTestHelper::RunClientMain(
    const base::Callback<int(MojoHandle)>& main,
    bool pass_pipe_ownership_to_main) {
  return RunClientFunction(
      [main](MojoHandle handle) { return main.Run(handle); },
      pass_pipe_ownership_to_main);
}

// static
int MultiprocessTestHelper::RunClientTestMain(
    const base::Callback<void(MojoHandle)>& main) {
  return RunClientFunction(
      [main](MojoHandle handle) {
        main.Run(handle);
        return (::testing::Test::HasFatalFailure() ||
                ::testing::Test::HasNonfatalFailure())
                   ? 1
                   : 0;
      },
      true /* pass_pipe_ownership_to_main */);
}

ScopedMessagePipeHandle MultiprocessTestHelper::primordial_pipe;

}  // namespace test
}  // namespace core
}  // namespace mojo
