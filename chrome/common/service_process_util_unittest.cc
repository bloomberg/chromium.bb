// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"

#if !defined(OS_MACOSX)
// TODO(dmaclach): Figure out tests that will work with launchd on Mac OS.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/service_process_util.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(OS_LINUX)
#include <glib.h>
#include "chrome/common/auto_start_linux.h"
#endif

namespace {

bool g_good_shutdown = false;

void ShutdownTask(MessageLoop* loop) {
  // Quit the main message loop.
  ASSERT_FALSE(g_good_shutdown);
  g_good_shutdown = true;
  loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

}  // namespace

TEST(ServiceProcessUtilTest, ScopedVersionedName) {
  std::string test_str = "test";
  std::string scoped_name = GetServiceProcessScopedVersionedName(test_str);
  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());
  EXPECT_TRUE(EndsWith(scoped_name, test_str, true));
  EXPECT_NE(std::string::npos, scoped_name.find(version_info.Version()));
}

class ServiceProcessStateTest : public base::MultiProcessTest {
 public:
  ServiceProcessStateTest();
  ~ServiceProcessStateTest();
  virtual void SetUp();
  base::MessageLoopProxy* IOMessageLoopProxy() {
    return io_thread_.message_loop_proxy();
  }
  void LaunchAndWait(const std::string& name);

 private:
  // This is used to release the ServiceProcessState singleton after each test.
  base::ShadowingAtExitManager at_exit_manager_;
  base::Thread io_thread_;
};

ServiceProcessStateTest::ServiceProcessStateTest()
    : io_thread_("ServiceProcessStateTestThread") {
}

ServiceProcessStateTest::~ServiceProcessStateTest() {
}

void ServiceProcessStateTest::SetUp() {
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  ASSERT_TRUE(io_thread_.StartWithOptions(options));
}

void ServiceProcessStateTest::LaunchAndWait(const std::string& name) {
  base::ProcessHandle handle = SpawnChild(name, false);
  ASSERT_TRUE(handle);
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCode(handle, &exit_code));
  ASSERT_EQ(exit_code, 0);
}

TEST_F(ServiceProcessStateTest, Singleton) {
  ServiceProcessState* state = ServiceProcessState::GetInstance();
  ASSERT_TRUE(state->Initialize());
  LaunchAndWait("ServiceProcessStateTestSingleton");
}

TEST_F(ServiceProcessStateTest, ReadyState) {
  ASSERT_FALSE(CheckServiceProcessReady());
  ServiceProcessState* state = ServiceProcessState::GetInstance();
  ASSERT_TRUE(state->Initialize());
  ASSERT_TRUE(state->SignalReady(IOMessageLoopProxy(), NULL));
  LaunchAndWait("ServiceProcessStateTestReadyTrue");
  state->SignalStopped();
  LaunchAndWait("ServiceProcessStateTestReadyFalse");
}

TEST_F(ServiceProcessStateTest, AutoRun) {
  ServiceProcessState* state = ServiceProcessState::GetInstance();
  ASSERT_TRUE(state->AddToAutoRun());
  scoped_ptr<CommandLine> autorun_command_line;
#if defined(OS_WIN)
  std::string value_name = GetServiceProcessScopedName("_service_run");
  string16 value;
  EXPECT_TRUE(base::win::ReadCommandFromAutoRun(HKEY_CURRENT_USER,
                                                UTF8ToWide(value_name),
                                                &value));
  autorun_command_line.reset(new CommandLine(CommandLine::FromString(value)));
#elif defined(OS_LINUX)
#if defined(GOOGLE_CHROME_BUILD)
  std::string base_desktop_name = "google-chrome-service.desktop";
#else  // CHROMIUM_BUILD
  std::string base_desktop_name = "chromium-service.desktop";
#endif
  std::string exec_value;
  EXPECT_TRUE(AutoStart::GetAutostartFileValue(
      GetServiceProcessScopedName(base_desktop_name), "Exec", &exec_value));
  GError *error = NULL;
  gchar **argv = NULL;
  gint argc = NULL;
  if (g_shell_parse_argv(exec_value.c_str(), &argc, &argv, &error)) {
    autorun_command_line.reset(new CommandLine(argc, argv));
    g_strfreev(argv);
  } else {
    ADD_FAILURE();
    g_error_free(error);
  }
#endif  // defined(OS_WIN)
  if (autorun_command_line.get()) {
    EXPECT_EQ(autorun_command_line->GetSwitchValueASCII(switches::kProcessType),
              std::string(switches::kServiceProcess));
  }
  ASSERT_TRUE(state->RemoveFromAutoRun());
#if defined(OS_WIN)
  EXPECT_FALSE(base::win::ReadCommandFromAutoRun(HKEY_CURRENT_USER,
                                                 UTF8ToWide(value_name),
                                                 &value));
#elif defined(OS_LINUX)
  EXPECT_FALSE(AutoStart::GetAutostartFileValue(
      GetServiceProcessScopedName(base_desktop_name), "Exec", &exec_value));
#endif  // defined(OS_WIN)
}

TEST_F(ServiceProcessStateTest, SharedMem) {
  std::string version;
  base::ProcessId pid;
#if defined(OS_WIN)
  // On Posix, named shared memory uses a file on disk. This file
  // could be lying around from previous crashes which could cause
  // GetServiceProcessPid to lie. On Windows, we use a named event so we
  // don't have this issue. Until we have a more stable shared memory
  // implementation on Posix, this check will only execute on Windows.
  ASSERT_FALSE(GetServiceProcessData(&version, &pid));
#endif  // defined(OS_WIN)
  ServiceProcessState* state = ServiceProcessState::GetInstance();
  ASSERT_TRUE(state->Initialize());
  ASSERT_TRUE(GetServiceProcessData(&version, &pid));
  ASSERT_EQ(base::GetCurrentProcId(), pid);
}

TEST_F(ServiceProcessStateTest, ForceShutdown) {
  base::ProcessHandle handle = SpawnChild("ServiceProcessStateTestShutdown",
                                          true);
  ASSERT_TRUE(handle);
  for (int i = 0; !CheckServiceProcessReady() && i < 10; ++i) {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout_ms());
  }
  ASSERT_TRUE(CheckServiceProcessReady());
  std::string version;
  base::ProcessId pid;
  ASSERT_TRUE(GetServiceProcessData(&version, &pid));
  ASSERT_TRUE(ForceServiceProcessShutdown(version, pid));
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCodeWithTimeout(handle,
      &exit_code, TestTimeouts::action_timeout_ms() * 2));
  ASSERT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestSingleton) {
  ServiceProcessState* state = ServiceProcessState::GetInstance();
  EXPECT_FALSE(state->Initialize());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestReadyTrue) {
  EXPECT_TRUE(CheckServiceProcessReady());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestReadyFalse) {
  EXPECT_FALSE(CheckServiceProcessReady());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestShutdown) {
  MessageLoop message_loop;
  message_loop.set_thread_name("ServiceProcessStateTestShutdownMainThread");
  base::Thread io_thread_("ServiceProcessStateTestShutdownIOThread");
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  EXPECT_TRUE(io_thread_.StartWithOptions(options));
  ServiceProcessState* state = ServiceProcessState::GetInstance();
  EXPECT_TRUE(state->Initialize());
  EXPECT_TRUE(state->SignalReady(io_thread_.message_loop_proxy(),
                                 NewRunnableFunction(&ShutdownTask,
                                                     MessageLoop::current())));
  message_loop.PostDelayedTask(FROM_HERE,
                               new MessageLoop::QuitTask(),
                               TestTimeouts::action_max_timeout_ms());
  EXPECT_FALSE(g_good_shutdown);
  message_loop.Run();
  EXPECT_TRUE(g_good_shutdown);
  return 0;
}

#endif  // !OS_MACOSX
