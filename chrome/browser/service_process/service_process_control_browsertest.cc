// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service_process/service_process_control.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/process/process_iterator.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/service_process_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

class ServiceProcessControlBrowserTest
    : public InProcessBrowserTest {
 public:
  ServiceProcessControlBrowserTest()
      : service_process_handle_(base::kNullProcessHandle) {
  }
  virtual ~ServiceProcessControlBrowserTest() {
    base::CloseProcessHandle(service_process_handle_);
    service_process_handle_ = base::kNullProcessHandle;
  }

  void HistogramsCallback() {
    MockHistogramsCallback();
    QuitMessageLoop();
  }

  MOCK_METHOD0(MockHistogramsCallback, void());

 protected:
  void LaunchServiceProcessControl() {
    // Launch the process asynchronously.
    ServiceProcessControl::GetInstance()->Launch(
        base::Bind(&ServiceProcessControlBrowserTest::ProcessControlLaunched,
                   this),
        base::Bind(
            &ServiceProcessControlBrowserTest::ProcessControlLaunchFailed,
            this));

    // Then run the message loop to keep things running.
    content::RunMessageLoop();
  }

  static void QuitMessageLoop() {
    base::MessageLoop::current()->Quit();
  }

  static void CloudPrintInfoCallback(
      const cloud_print::CloudPrintProxyInfo& proxy_info) {
    QuitMessageLoop();
  }

  void Disconnect() {
    // This will close the IPC connection.
    ServiceProcessControl::GetInstance()->Disconnect();
  }

  virtual void SetUp() OVERRIDE {
    service_process_handle_ = base::kNullProcessHandle;
  }

  virtual void TearDown() OVERRIDE {
    if (ServiceProcessControl::GetInstance()->IsConnected())
      EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
#if defined(OS_MACOSX)
    // ForceServiceProcessShutdown removes the process from launched on Mac.
    ForceServiceProcessShutdown("", 0);
#endif  // OS_MACOSX
    if (service_process_handle_ != base::kNullProcessHandle) {
      EXPECT_TRUE(base::WaitForSingleProcess(
          service_process_handle_,
          TestTimeouts::action_max_timeout()));
      service_process_handle_ = base::kNullProcessHandle;
    }
  }

  void ProcessControlLaunched() {
    base::ProcessId service_pid;
    EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
    EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
    EXPECT_TRUE(base::OpenProcessHandleWithAccess(
        service_pid,
        base::kProcessAccessWaitForTermination |
        // we need query permission to get exit code
        base::kProcessAccessQueryInformation,
        &service_process_handle_));
    // Quit the current message. Post a QuitTask instead of just calling Quit()
    // because this can get invoked in the context of a Launch() call and we
    // may not be in Run() yet.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }

  void ProcessControlLaunchFailed() {
    ADD_FAILURE();
    // Quit the current message.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }

 private:
  base::ProcessHandle service_process_handle_;
};

class RealServiceProcessControlBrowserTest
      : public ServiceProcessControlBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ServiceProcessControlBrowserTest::SetUpCommandLine(command_line);
    base::FilePath exe;
    PathService::Get(base::DIR_EXE, &exe);
#if defined(OS_MACOSX)
    exe = exe.DirName().DirName().DirName();
#endif
    exe = exe.Append(chrome::kHelperProcessExecutablePath);
    // Run chrome instead of browser_tests.exe.
    EXPECT_TRUE(base::PathExists(exe));
    command_line->AppendSwitchPath(switches::kBrowserSubprocessPath, exe);
  }
};

// TODO(vitalybuka): Fix crbug.com/340563
IN_PROC_BROWSER_TEST_F(RealServiceProcessControlBrowserTest,
                       DISABLED_LaunchAndIPC) {
  LaunchServiceProcessControl();

  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  ServiceProcessControl::GetInstance()->GetCloudPrintProxyInfo(
        base::Bind(&ServiceProcessControlBrowserTest::CloudPrintInfoCallback));
  content::RunMessageLoop();

  // And then shutdown the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, LaunchAndIPC) {
  LaunchServiceProcessControl();

  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  ServiceProcessControl::GetInstance()->GetCloudPrintProxyInfo(
        base::Bind(&ServiceProcessControlBrowserTest::CloudPrintInfoCallback));
  content::RunMessageLoop();

  // And then shutdown the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
}

// This tests the case when a service process is launched when the browser
// starts but we try to launch it again while setting up Cloud Print.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, LaunchTwice) {
  // Launch the service process the first time.
  LaunchServiceProcessControl();

  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->GetCloudPrintProxyInfo(
        base::Bind(&ServiceProcessControlBrowserTest::CloudPrintInfoCallback)));
  content::RunMessageLoop();

  // Launch the service process again.
  LaunchServiceProcessControl();
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->GetCloudPrintProxyInfo(
        base::Bind(&ServiceProcessControlBrowserTest::CloudPrintInfoCallback)));
  content::RunMessageLoop();
}

static void DecrementUntilZero(int* count) {
  (*count)--;
  if (!(*count))
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
}

// Invoke multiple Launch calls in succession and ensure that all the tasks
// get invoked.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest,
                       MultipleLaunchTasks) {
  ServiceProcessControl* process = ServiceProcessControl::GetInstance();
  int launch_count = 5;
  for (int i = 0; i < launch_count; i++) {
    // Launch the process asynchronously.
    process->Launch(base::Bind(&DecrementUntilZero, &launch_count),
                    base::MessageLoop::QuitClosure());
  }
  // Then run the message loop to keep things running.
  content::RunMessageLoop();
  EXPECT_EQ(0, launch_count);
}

// Make sure using the same task for success and failure tasks works.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, SameLaunchTask) {
  ServiceProcessControl* process = ServiceProcessControl::GetInstance();
  int launch_count = 5;
  for (int i = 0; i < launch_count; i++) {
    // Launch the process asynchronously.
    base::Closure task = base::Bind(&DecrementUntilZero, &launch_count);
    process->Launch(task, task);
  }
  // Then run the message loop to keep things running.
  content::RunMessageLoop();
  EXPECT_EQ(0, launch_count);
}

// Tests whether disconnecting from the service IPC causes the service process
// to die.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, DieOnDisconnect) {
  // Launch the service process.
  LaunchServiceProcessControl();
  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  Disconnect();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, ForceShutdown) {
  // Launch the service process.
  LaunchServiceProcessControl();
  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  base::ProcessId service_pid;
  EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
  EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
  chrome::VersionInfo version_info;
  ForceServiceProcessShutdown(version_info.Version(), service_pid);
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, CheckPid) {
  base::ProcessId service_pid;
  EXPECT_FALSE(GetServiceProcessData(NULL, &service_pid));
  // Launch the service process.
  LaunchServiceProcessControl();
  EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
  EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
  // Disconnect from service process.
  Disconnect();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, HistogramsNoService) {
  ASSERT_FALSE(ServiceProcessControl::GetInstance()->IsConnected());
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(0);
  EXPECT_FALSE(ServiceProcessControl::GetInstance()->GetHistograms(
      base::Bind(&ServiceProcessControlBrowserTest::HistogramsCallback,
                 base::Unretained(this)),
      base::TimeDelta()));
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, HistogramsTimeout) {
  LaunchServiceProcessControl();
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  // Callback should not be called during GetHistograms call.
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(0);
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->GetHistograms(
      base::Bind(&ServiceProcessControlBrowserTest::HistogramsCallback,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(100)));
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(1);
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
  content::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, Histograms) {
  LaunchServiceProcessControl();
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  // Callback should not be called during GetHistograms call.
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(0);
  // Wait for real callback by providing large timeout value.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->GetHistograms(
      base::Bind(&ServiceProcessControlBrowserTest::HistogramsCallback,
                base::Unretained(this)),
      base::TimeDelta::FromHours(1)));
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(1);
  content::RunMessageLoop();
}
