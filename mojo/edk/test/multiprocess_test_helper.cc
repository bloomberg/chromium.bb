// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/multiprocess_test_helper.h"

#include <functional>
#include <set>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace mojo {
namespace edk {
namespace test {

namespace {

const char kMojoPrimordialPipeToken[] = "mojo-primordial-pipe-token";

void RunHandlerOnMainThread(std::function<int(MojoHandle)> handler,
                            int* exit_code,
                            const base::Closure& quit_closure,
                            ScopedMessagePipeHandle pipe) {
  *exit_code = handler(pipe.get().value());
  quit_closure.Run();
}

void RunHandler(std::function<int(MojoHandle)> handler,
                int* exit_code,
                const base::Closure& quit_closure,
                scoped_refptr<base::TaskRunner> task_runner,
                ScopedMessagePipeHandle pipe) {
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&RunHandlerOnMainThread, handler, base::Unretained(exit_code),
                 quit_closure, base::Passed(&pipe)));
}

int RunClientFunction(std::function<int(MojoHandle)> handler) {
  base::RunLoop run_loop;
  int exit_code = 0;
  CHECK(!MultiprocessTestHelper::primordial_pipe_token.empty());
  CreateChildMessagePipe(
      MultiprocessTestHelper::primordial_pipe_token,
      base::Bind(&RunHandler, handler, base::Unretained(&exit_code),
                 run_loop.QuitClosure(), base::ThreadTaskRunnerHandle::Get()));
  run_loop.Run();
  return exit_code;
}

}  // namespace

MultiprocessTestHelper::MultiprocessTestHelper() {}

MultiprocessTestHelper::~MultiprocessTestHelper() {
  CHECK(!test_child_.IsValid());
}

void MultiprocessTestHelper::StartChild(const std::string& test_child_name,
                                        const HandlerCallback& callback) {
  StartChildWithExtraSwitch(
      test_child_name, std::string(), std::string(), callback);
}

void MultiprocessTestHelper::StartChildWithExtraSwitch(
    const std::string& test_child_name,
    const std::string& switch_string,
    const std::string& switch_value,
    const HandlerCallback& callback) {
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

  PlatformChannelPair channel;
  HandlePassingInformation handle_passing_info;
  channel.PrepareToPassClientHandleToChildProcess(&command_line,
                                                  &handle_passing_info);

  std::string pipe_token = mojo::edk::GenerateRandomToken();
  command_line.AppendSwitchASCII(kMojoPrimordialPipeToken, pipe_token);

  if (!switch_string.empty()) {
    CHECK(!command_line.HasSwitch(switch_string));
    if (!switch_value.empty())
      command_line.AppendSwitchASCII(switch_string, switch_value);
    else
      command_line.AppendSwitch(switch_string);
  }

  base::LaunchOptions options;
#if defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#elif defined(OS_WIN)
  options.start_hidden = true;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    options.handles_to_inherit = &handle_passing_info;
  else
    options.inherit_handles = true;
#else
#error "Not supported yet."
#endif

  test_child_ =
      base::SpawnMultiProcessTestChild(test_child_main, command_line, options);
  channel.ChildProcessLaunched();

  ChildProcessLaunched(test_child_.Handle(), channel.PassServerHandle());
  CreateParentMessagePipe(pipe_token, callback);

  CHECK(test_child_.IsValid());
}

int MultiprocessTestHelper::WaitForChildShutdown() {
  CHECK(test_child_.IsValid());

  int rv = -1;
  CHECK(
      test_child_.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &rv));
  test_child_.Close();
  return rv;
}

bool MultiprocessTestHelper::WaitForChildTestShutdown() {
  return WaitForChildShutdown() == 0;
}

// static
void MultiprocessTestHelper::ChildSetup() {
  CHECK(base::CommandLine::InitializedForCurrentProcess());

  primordial_pipe_token = base::CommandLine::ForCurrentProcess()
      ->GetSwitchValueASCII(kMojoPrimordialPipeToken);
  CHECK(!primordial_pipe_token.empty());

  SetParentPipeHandle(
      PlatformChannelPair::PassClientHandleFromParentProcess(
          *base::CommandLine::ForCurrentProcess()));
}

// static
int MultiprocessTestHelper::RunClientMain(
    const base::Callback<int(MojoHandle)>& main) {
  return RunClientFunction([main](MojoHandle handle){
    return main.Run(handle);
  });
}

// static
int MultiprocessTestHelper::RunClientTestMain(
    const base::Callback<void(MojoHandle)>& main) {
  return RunClientFunction([main](MojoHandle handle) {
    main.Run(handle);
    return (::testing::Test::HasFatalFailure() ||
            ::testing::Test::HasNonfatalFailure()) ? 1 : 0;
  });
}

// static
std::string MultiprocessTestHelper::primordial_pipe_token;

}  // namespace test
}  // namespace edk
}  // namespace mojo
