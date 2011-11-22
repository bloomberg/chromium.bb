// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/process_util.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/service_process_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

class ServiceProcessControlBrowserTest
    : public InProcessBrowserTest {
 public:
  ServiceProcessControlBrowserTest()
      : service_process_handle_(base::kNullProcessHandle) {
  }
  ~ServiceProcessControlBrowserTest() {
    base::CloseProcessHandle(service_process_handle_);
    service_process_handle_ = base::kNullProcessHandle;
  }

#if defined(OS_MACOSX)
  virtual void TearDown() {
    // ForceServiceProcessShutdown removes the process from launched on Mac.
    ForceServiceProcessShutdown("", 0);
  }
#endif  // OS_MACOSX

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
    ui_test_utils::RunMessageLoop();
  }

  // Send a Cloud Print status request and wait for a reply from the service.
  void SendRequestAndWait() {
    ServiceProcessControl::GetInstance()->GetCloudPrintProxyInfo(
        base::Bind(&ServiceProcessControlBrowserTest::CloudPrintInfoCallback,
                   base::Unretained(this)));
    ui_test_utils::RunMessageLoop();
  }

  void CloudPrintInfoCallback(
      const cloud_print::CloudPrintProxyInfo& proxy_info) {
    MessageLoop::current()->Quit();
  }

  void Disconnect() {
    // This will close the IPC connection.
    ServiceProcessControl::GetInstance()->Disconnect();
  }

  void WaitForShutdown() {
    EXPECT_TRUE(base::WaitForSingleProcess(
        service_process_handle_,
        TestTimeouts::action_max_timeout_ms()));
  }

  void ProcessControlLaunched() {
    base::ProcessId service_pid;
    EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
    EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
    EXPECT_TRUE(base::OpenProcessHandleWithAccess(
        service_pid,
        base::kProcessAccessWaitForTermination,
        &service_process_handle_));
    // Quit the current message. Post a QuitTask instead of just calling Quit()
    // because this can get invoked in the context of a Launch() call and we
    // may not be in Run() yet.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  void ProcessControlLaunchFailed() {
    ADD_FAILURE();
    // Quit the current message.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

 private:
  base::ProcessHandle service_process_handle_;
};

// They way that the IPC is implemented only works on windows. This has to
// change when we implement a different scheme for IPC.
// Times out flakily, http://crbug.com/70076.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest,
                       DISABLED_LaunchAndIPC) {
  LaunchServiceProcessControl();

  // Make sure we are connected to the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  SendRequestAndWait();

  // And then shutdown the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
}

// This tests the case when a service process is launched when the browser
// starts but we try to launch it again while setting up Cloud Print.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, LaunchTwice) {
  // Launch the service process the first time.
  LaunchServiceProcessControl();

  // Make sure we are connected to the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  SendRequestAndWait();

  // Launch the service process again.
  LaunchServiceProcessControl();
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  SendRequestAndWait();

  // And then shutdown the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
}

static void DecrementUntilZero(int* count) {
  (*count)--;
  if (!(*count))
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
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
                    MessageLoop::QuitClosure());
  }
  // Then run the message loop to keep things running.
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(0, launch_count);
  // And then shutdown the service process.
  EXPECT_TRUE(process->Shutdown());
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
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(0, launch_count);
  // And then shutdown the service process.
  EXPECT_TRUE(process->Shutdown());
}

// Tests whether disconnecting from the service IPC causes the service process
// to die.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest,
                       DieOnDisconnect) {
  // Launch the service process.
  LaunchServiceProcessControl();
  // Make sure we are connected to the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  Disconnect();
  WaitForShutdown();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest,
                       ForceShutdown) {
  // Launch the service process.
  LaunchServiceProcessControl();
  // Make sure we are connected to the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  base::ProcessId service_pid;
  EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
  EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
  chrome::VersionInfo version_info;
  ForceServiceProcessShutdown(version_info.Version(), service_pid);
  WaitForShutdown();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, CheckPid) {
  base::ProcessId service_pid;
  EXPECT_FALSE(GetServiceProcessData(NULL, &service_pid));
  // Launch the service process.
  LaunchServiceProcessControl();
  EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
  EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
  // Disconnect from service process.
  ServiceProcessControl::GetInstance()->Disconnect();
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(ServiceProcessControlBrowserTest);
