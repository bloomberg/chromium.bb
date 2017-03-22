// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray.h"
#include "ash/shell.h"
#include "base/strings/pattern.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/speech_monitor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/notification_types.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace chromeos {

class SelectToSpeakTest : public InProcessBrowserTest {
 protected:
  SelectToSpeakTest() {}
  ~SelectToSpeakTest() override {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());

    content::WindowedNotificationObserver extension_load_waiter(
        extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
        content::NotificationService::AllSources());
    AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    extension_load_waiter.Wait();

    aura::Window* root_window =
        ash::Shell::GetInstance()->GetPrimaryRootWindow();
    generator_.reset(new ui::test::EventGenerator(root_window));

    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  SpeechMonitor speech_monitor_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakTest);
};

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SpeakStatusTray) {
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  gfx::Rect tray_bounds = tray->GetBoundsInScreen();

  // Hold down Search and click a few pixels into the status tray bounds.
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->MoveMouseTo(tray_bounds.x() + 8, tray_bounds.y() + 8);
  generator_->PressLeftButton();
  generator_->ReleaseLeftButton();
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);

  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "Status tray*"));
}

}  // namespace chromeos
