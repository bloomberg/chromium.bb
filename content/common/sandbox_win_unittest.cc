// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/multiprocess_test.h"
#include "base/win/win_util.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/security_level.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace content {
namespace {

const size_t kMaxMessageLength = 64;
const char kTestMessage1Switch[] = "sandbox-test-message-1";
const char kTestMessage2Switch[] = "sandbox-test-message-2";
const char kInheritedHandle1Switch[] = "inherited-handle-1";
const char kInheritedHandle2Switch[] = "inherited-handle-2";

void InitializeTestSandbox() {
  sandbox::SandboxInterfaceInfo info = {0};
  info.broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (!info.broker_services)
    info.target_services = sandbox::SandboxFactory::GetTargetServices();
  CHECK(info.broker_services || info.target_services);
  CHECK(InitializeSandbox(&info));
}

class SandboxInitializer {
 public:
  SandboxInitializer() { InitializeTestSandbox(); }
};

base::LazyInstance<SandboxInitializer> g_sandbox_initializer;

HANDLE GetHandleFromCommandLine(const char* switch_name) {
  unsigned handle_id;
  CHECK(base::StringToUint(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switch_name),
      &handle_id));
  CHECK(handle_id);
  return reinterpret_cast<HANDLE>(handle_id);
}

std::string ReadMessageFromPipe(HANDLE pipe) {
  char buffer[kMaxMessageLength];
  DWORD bytes_read = 0;
  BOOL result =
      ::ReadFile(pipe, buffer, kMaxMessageLength, &bytes_read, nullptr);
  PCHECK(result);
  return std::string(buffer, bytes_read);
}

void WriteMessageToPipe(HANDLE pipe, const base::StringPiece& message) {
  DWORD bytes_written = 0;
  BOOL result = ::WriteFile(pipe, message.data(), message.size(),
                            &bytes_written, nullptr);
  CHECK_EQ(message.size(), static_cast<size_t>(bytes_written));
  PCHECK(result);
}

// Does what MultiProcessTest::MakeCmdLine does, plus ensures ".exe" extension
// on Program.
base::CommandLine MakeCmdLineWithExtension(const std::string& testTarget) {
  base::CommandLine cmd_line = base::GetMultiProcessTestChildBaseCommandLine();
  cmd_line.AppendSwitchASCII(switches::kTestChildProcess, testTarget);

  base::FilePath program = cmd_line.GetProgram();
  cmd_line.SetProgram(program.ReplaceExtension(L"exe"));
  return cmd_line;
}

class SandboxWinTest : public base::MultiProcessTest {
 public:
  SandboxWinTest() {}
  ~SandboxWinTest() override {}

  void SetUp() override {
    // Ensure the sandbox broker services are initialized exactly once in the
    // parent test process.
    g_sandbox_initializer.Get();
  }

 protected:
  // Creates a new pipe for synchronous IO. The client handle is created in a
  // non-inheritable state.
  void CreatePipe(base::win::ScopedHandle* server,
                  base::win::ScopedHandle* client) {
    std::string pipe_name = base::StringPrintf(
        "\\\\.\\pipe\\sandbox_win_test.%I64u", base::RandUint64());
    server->Set(CreateNamedPipeA(
        pipe_name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                               FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, 4096, 4096, 5000, nullptr));
    CHECK(server->IsValid());

    client->Set(CreateFileA(
        pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr));
    CHECK(client->IsValid());
  }

  base::Process LaunchTestClient(
      base::CommandLine* cmd_line,
      bool sandboxed,
      const base::HandlesToInheritVector& handles_to_inherit) {
    TestLauncherDelegate delegate(sandboxed);
    base::Process process = StartSandboxedProcess(
        &delegate, cmd_line, handles_to_inherit);
    CHECK(process.IsValid());
    return process;
  }

  class TestLauncherDelegate : public SandboxedProcessLauncherDelegate {
   public:
    explicit TestLauncherDelegate(bool should_sandbox)
        : should_sandbox_(should_sandbox) {}
    ~TestLauncherDelegate() override {}

    // SandboxedProcessLauncherDelegate:
    bool ShouldSandbox() override { return should_sandbox_; }

    bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
      policy->SetTokenLevel(sandbox::USER_INTERACTIVE, sandbox::USER_LOCKDOWN);
      return true;
    }

    SandboxType GetSandboxType() override { return SANDBOX_TYPE_RENDERER; }

   private:
    bool should_sandbox_;
  };
};

// -----------------------------------------------------------------------------
// Tests follow: single-handle inheritance.
// -----------------------------------------------------------------------------

MULTIPROCESS_TEST_MAIN(ReadPipe) {
  InitializeTestSandbox();

  // Expects a message and pipe handle to be passed in, and expects to read the
  // same message from that pipe. Exits with 0 on success, 1 on failure.

  base::win::ScopedHandle pipe(
      GetHandleFromCommandLine(kInheritedHandle1Switch));
  std::string message = ReadMessageFromPipe(pipe.Get());

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kTestMessage1Switch) != message) {
    return 1;
  }

  return 0;
}

TEST_F(SandboxWinTest, InheritSingleHandleUnsandboxed) {
  base::CommandLine cmd_line = MakeCmdLineWithExtension("ReadPipe");

  base::win::ScopedHandle server, client;
  CreatePipe(&server, &client);

  base::HandlesToInheritVector handles;
  handles.push_back(client.Get());

  // Pass a test message and the pipe handle ID on the command line.
  const std::string kTestMessage = "Hello, world!";
  cmd_line.AppendSwitchASCII(kTestMessage1Switch, kTestMessage);
  cmd_line.AppendSwitchASCII(
      kInheritedHandle1Switch,
      base::UintToString(base::win::HandleToUint32(client.Get())));

  base::Process child_process =
      LaunchTestClient(&cmd_line, false /* sandboxed */, handles);

  WriteMessageToPipe(server.Get(), kTestMessage);

  int exit_code = 0;
  child_process.WaitForExit(&exit_code);
  EXPECT_EQ(0, exit_code);
}

TEST_F(SandboxWinTest, InheritSingleHandleSandboxed) {
  base::CommandLine cmd_line = MakeCmdLineWithExtension("ReadPipe");

  base::win::ScopedHandle server, client;
  CreatePipe(&server, &client);

  base::HandlesToInheritVector handles;
  handles.push_back(client.Get());

  // Pass a test message and the pipe handle ID on the command line.
  const std::string kTestMessage = "Hello, world!";
  cmd_line.AppendSwitchASCII(kTestMessage1Switch, kTestMessage);
  cmd_line.AppendSwitchASCII(
      kInheritedHandle1Switch,
      base::UintToString(base::win::HandleToUint32(client.Get())));

  base::Process child_process =
      LaunchTestClient(&cmd_line, true /* sandboxed */, handles);

  WriteMessageToPipe(server.Get(), kTestMessage);

  int exit_code = 0;
  child_process.WaitForExit(&exit_code);
  EXPECT_EQ(0, exit_code);
}

// -----------------------------------------------------------------------------
// Tests follow: multi-handle inheritance.
// -----------------------------------------------------------------------------

MULTIPROCESS_TEST_MAIN(ReadMultiPipes) {
  InitializeTestSandbox();

  // Expects two messages and two pipe handles to be passed in, and expects to
  // read each message from its respective pipe.  Exits with 0 on success, 1 on
  // failure.

  base::win::ScopedHandle pipe1(
      GetHandleFromCommandLine(kInheritedHandle1Switch));
  std::string message1 = ReadMessageFromPipe(pipe1.Get());

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kTestMessage1Switch) != message1) {
    return 1;
  }

  base::win::ScopedHandle pipe2(
      GetHandleFromCommandLine(kInheritedHandle2Switch));
  std::string message2 = ReadMessageFromPipe(pipe2.Get());

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kTestMessage2Switch) != message2) {
    return 1;
  }

  return 0;
}

TEST_F(SandboxWinTest, InheritMultipleHandlesUnsandboxed) {
  base::CommandLine cmd_line = MakeCmdLineWithExtension("ReadMultiPipes");

  base::win::ScopedHandle server1, client1;
  CreatePipe(&server1, &client1);

  base::win::ScopedHandle server2, client2;
  CreatePipe(&server2, &client2);

  base::HandlesToInheritVector handles;
  handles.push_back(client1.Get());
  handles.push_back(client2.Get());

  // Pass each pipe handle ID and a message for each on the command line.
  const std::string kTestMessage1 = "Hello, world!";
  const std::string kTestMessage2 = "Goodbye, world!";

  cmd_line.AppendSwitchASCII(kTestMessage1Switch, kTestMessage1);
  cmd_line.AppendSwitchASCII(
      kInheritedHandle1Switch,
      base::UintToString(base::win::HandleToUint32(client1.Get())));

  cmd_line.AppendSwitchASCII(kTestMessage2Switch, kTestMessage2);
  cmd_line.AppendSwitchASCII(
      kInheritedHandle2Switch,
      base::UintToString(base::win::HandleToUint32(client2.Get())));

  base::Process child_process =
      LaunchTestClient(&cmd_line, false /* sandboxed */, handles);

  WriteMessageToPipe(server1.Get(), kTestMessage1);
  WriteMessageToPipe(server2.Get(), kTestMessage2);

  int exit_code = 0;
  child_process.WaitForExit(&exit_code);
  EXPECT_EQ(0, exit_code);
}

TEST_F(SandboxWinTest, InheritMultipleHandlesSandboxed) {
  base::CommandLine cmd_line = MakeCmdLineWithExtension("ReadMultiPipes");

  base::win::ScopedHandle server1, client1;
  CreatePipe(&server1, &client1);

  base::win::ScopedHandle server2, client2;
  CreatePipe(&server2, &client2);

  base::HandlesToInheritVector handles;
  handles.push_back(client1.Get());
  handles.push_back(client2.Get());

  // Pass each pipe handle ID and a message for each on the command line.
  const std::string kTestMessage1 = "Hello, world!";
  const std::string kTestMessage2 = "Goodbye, world!";

  cmd_line.AppendSwitchASCII(kTestMessage1Switch, kTestMessage1);
  cmd_line.AppendSwitchASCII(
      kInheritedHandle1Switch,
      base::UintToString(base::win::HandleToUint32(client1.Get())));

  cmd_line.AppendSwitchASCII(kTestMessage2Switch, kTestMessage2);
  cmd_line.AppendSwitchASCII(
      kInheritedHandle2Switch,
      base::UintToString(base::win::HandleToUint32(client2.Get())));

  base::Process child_process =
      LaunchTestClient(&cmd_line, true /* sandboxed */, handles);

  WriteMessageToPipe(server1.Get(), kTestMessage1);
  WriteMessageToPipe(server2.Get(), kTestMessage2);

  int exit_code = 0;
  child_process.WaitForExit(&exit_code);
  EXPECT_EQ(0, exit_code);
}

}  // namespace
}  // namespace
