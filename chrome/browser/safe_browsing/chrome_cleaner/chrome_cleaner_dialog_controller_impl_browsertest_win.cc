// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_impl_win.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;
using ::testing::Return;

class MockChromeCleanerPromptDelegate : public ChromeCleanerPromptDelegate {
 public:
  MOCK_METHOD3(ShowChromeCleanerPrompt,
               void(Browser* browser,
                    ChromeCleanerDialogController* dialog_controller,
                    ChromeCleanerController* cleaner_controller));
};

class MockChromeCleanerController
    : public safe_browsing::ChromeCleanerController {
 public:
  MOCK_METHOD0(ShouldShowCleanupInSettingsUI, bool());
  MOCK_METHOD0(IsPoweredByPartner, bool());
  MOCK_CONST_METHOD0(state, State());
  MOCK_CONST_METHOD0(idle_reason, IdleReason());
  MOCK_METHOD1(SetLogsEnabled, void(bool));
  MOCK_CONST_METHOD0(logs_enabled, bool());
  MOCK_METHOD0(ResetIdleState, void());
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(Scan, void(const safe_browsing::SwReporterInvocation&));
  MOCK_METHOD2(ReplyWithUserResponse, void(Profile*, UserResponse));
  MOCK_METHOD0(Reboot, void());
};

class ChromeCleanerPromptUserTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
// dialog_controller_ expects that the cleaner controller would be on
// scanning state.
#if DCHECK_IS_ON()
    EXPECT_CALL(mock_cleaner_controller_, state())
        .WillOnce(Return(ChromeCleanerController::State::kScanning));
#endif
    EXPECT_CALL(mock_cleaner_controller_, AddObserver(_));
    dialog_controller_ =
        new ChromeCleanerDialogControllerImpl(&mock_cleaner_controller_);
    dialog_controller_->SetPromptDelegateForTests(&mock_delegate_);
  }

 protected:
  MockChromeCleanerController mock_cleaner_controller_;
  ChromeCleanerDialogControllerImpl* dialog_controller_;
  StrictMock<MockChromeCleanerPromptDelegate> mock_delegate_;
};

IN_PROC_BROWSER_TEST_F(ChromeCleanerPromptUserTest,
                       OnInfectedBrowserAvailable) {
  EXPECT_CALL(mock_delegate_, ShowChromeCleanerPrompt(_, _, _)).Times(1);
  dialog_controller_->OnInfected(std::set<base::FilePath>());
}

IN_PROC_BROWSER_TEST_F(ChromeCleanerPromptUserTest,
                       DISABLED_OnInfectedBrowserNotAvailable) {
  browser()->window()->Minimize();
  base::RunLoop().RunUntilIdle();
  dialog_controller_->OnInfected(std::set<base::FilePath>());

  base::RunLoop run_loop;
  // We only set the expectation here because we want to make sure that the
  // prompt is shown only when the window is restored.
  EXPECT_CALL(mock_delegate_, ShowChromeCleanerPrompt(_, _, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  browser()->window()->Restore();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ChromeCleanerPromptUserTest, AllBrowsersClosed) {
  std::unique_ptr<ScopedKeepAlive> keep_alive =
      base::MakeUnique<ScopedKeepAlive>(KeepAliveOrigin::BROWSER,
                                        KeepAliveRestartOption::DISABLED);

  CloseAllBrowsers();
  base::RunLoop().RunUntilIdle();
  dialog_controller_->OnInfected(std::set<base::FilePath>());

  base::RunLoop run_loop;
  // We only set the expectation here because we want to make sure that the
  // prompt is shown only when the window is restored.
  EXPECT_CALL(mock_delegate_, ShowChromeCleanerPrompt(_, _, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  CreateBrowser(ProfileManager::GetActiveUserProfile());
  run_loop.Run();
}

}  // namespace
}  // namespace safe_browsing
