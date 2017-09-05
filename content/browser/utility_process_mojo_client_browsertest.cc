// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/utility_process_mojo_client.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_service.mojom.h"

namespace content {

// Test fixture used to make different Mojo calls to the utility process.
class UtilityProcessMojoClientBrowserTest : public ContentBrowserTest {
 public:
  enum RunOption {
    SANDBOXED, UNSANDBOXED, ELEVATED
  };

  void StartMojoService(RunOption option = SANDBOXED) {
    mojo_client_.reset(new UtilityProcessMojoClient<mojom::TestService>(
        base::ASCIIToUTF16("TestUtilityProcessMojoClient")));

    mojo_client_->set_error_callback(
        base::Bind(&UtilityProcessMojoClientBrowserTest::OnConnectionError,
                   base::Unretained(this)));

    // This test case needs to have the sandbox disabled.
    if (option == UNSANDBOXED)
      mojo_client_->set_disable_sandbox();
#if defined(OS_WIN)
    // This test case needs utility process UAC privilege elevation.
    if (option == ELEVATED)
      mojo_client_->set_run_elevated();
#endif  // defined(OS_WIN)

    mojo_client_->Start();
  }

  // Called when a response is received from a call to DoSomething() or
  // DoTerminateProcess().
  void OnResponseReceived() {
    response_received_ = true;
    done_closure_.Run();
  }

  // Called when a connection error happen between the test and the utility
  // process.
  void OnConnectionError() {
    error_happened_ = true;
    done_closure_.Run();
  }

  // Called when a responsed is received from a call to CreateFolder().
  void OnCreateFolderFinished(bool succeeded) {
    response_received_ = true;
    sandbox_succeeded_ = succeeded;
    done_closure_.Run();
  }

 protected:
  std::unique_ptr<UtilityProcessMojoClient<mojom::TestService>>
      mojo_client_;
  base::Closure done_closure_;

  // The test result that is compared to the current test case.
  bool response_received_ = false;
  bool error_happened_ = false;
  bool sandbox_succeeded_ = false;
};

// Successful call through the Mojo service with response back.
IN_PROC_BROWSER_TEST_F(UtilityProcessMojoClientBrowserTest, CallService) {
  base::RunLoop run_loop;
  done_closure_ = run_loop.QuitClosure();

  StartMojoService();

  mojo_client_->service()->DoSomething(
      base::BindOnce(&UtilityProcessMojoClientBrowserTest::OnResponseReceived,
                     base::Unretained(this)));

  run_loop.Run();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(error_happened_);
}

// Call the Mojo service but the utility process terminates before getting
// the result back.
IN_PROC_BROWSER_TEST_F(UtilityProcessMojoClientBrowserTest, ConnectionError) {
  base::RunLoop run_loop;
  done_closure_ = run_loop.QuitClosure();

  StartMojoService();

  mojo_client_->service()->DoTerminateProcess(
      base::BindOnce(&UtilityProcessMojoClientBrowserTest::OnResponseReceived,
                     base::Unretained(this)));

  run_loop.Run();
  EXPECT_FALSE(response_received_);
  EXPECT_TRUE(error_happened_);
}

// Android doesn't support non-sandboxed utility processes.
#if !defined(OS_ANDROID)
// Call a function that fails because the sandbox is still enabled.
IN_PROC_BROWSER_TEST_F(UtilityProcessMojoClientBrowserTest, SandboxFailure) {
  base::RunLoop run_loop;
  done_closure_ = run_loop.QuitClosure();

  StartMojoService();

  mojo_client_->service()->CreateFolder(base::BindOnce(
      &UtilityProcessMojoClientBrowserTest::OnCreateFolderFinished,
      base::Unretained(this)));

  run_loop.Run();
  EXPECT_TRUE(response_received_);
  // If the sandbox is disabled by the command line, this will succeed.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox))
    EXPECT_FALSE(sandbox_succeeded_);
  EXPECT_FALSE(error_happened_);
}

// Call a function that succeeds only when the sandbox is disabled.
IN_PROC_BROWSER_TEST_F(UtilityProcessMojoClientBrowserTest, SandboxSuccess) {
  base::RunLoop run_loop;
  done_closure_ = run_loop.QuitClosure();

  StartMojoService(UNSANDBOXED);

  mojo_client_->service()->CreateFolder(base::BindOnce(
      &UtilityProcessMojoClientBrowserTest::OnCreateFolderFinished,
      base::Unretained(this)));

  run_loop.Run();
  EXPECT_TRUE(response_received_);
  EXPECT_TRUE(sandbox_succeeded_);
  EXPECT_FALSE(error_happened_);
}
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
// Call a function that succeeds with process elevation. Elevation is only
// available on WIN, and is used to permit UAC prompts for operations that
// need user approval before proceeding.
IN_PROC_BROWSER_TEST_F(UtilityProcessMojoClientBrowserTest, ElevatedSuccess) {
  base::RunLoop run_loop;
  done_closure_ = run_loop.QuitClosure();

  StartMojoService(ELEVATED);

  mojo_client_->service()->CreateFolder(
      base::Bind(&UtilityProcessMojoClientBrowserTest::OnCreateFolderFinished,
                 base::Unretained(this)));

  run_loop.Run();
  EXPECT_TRUE(response_received_);
  EXPECT_TRUE(sandbox_succeeded_);
  EXPECT_FALSE(error_happened_);
}
#endif  // defined(OS_WIN)

}  // namespace content
