// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/profiles/new_avatar_button.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/test/test_utils.h"
#include "ui/views/controls/button/label_button.h"

class ProfileChooserViewBrowserTest : public InProcessBrowserTest {
 public:
  ProfileChooserViewBrowserTest();
  virtual ~ProfileChooserViewBrowserTest();

 protected:
  virtual void SetUp() OVERRIDE;
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  void OpenProfileChooserView();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileChooserViewBrowserTest);
};

ProfileChooserViewBrowserTest::ProfileChooserViewBrowserTest() {
}

ProfileChooserViewBrowserTest::~ProfileChooserViewBrowserTest() {
}

void ProfileChooserViewBrowserTest::SetUp() {
  InProcessBrowserTest::SetUp();
  DCHECK(switches::IsNewAvatarMenu());
}

void ProfileChooserViewBrowserTest::SetUpCommandLine(
    CommandLine* command_line) {
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());
}

void ProfileChooserViewBrowserTest::OpenProfileChooserView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  NewAvatarButton* button = browser_view->frame()->GetNewAvatarMenuButton();
  ASSERT_TRUE(button);

  ProfileChooserView::clear_close_on_deactivate_for_testing();
  ui::MouseEvent mouse_ev(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), 0,
                          0);
  button->NotifyClick(mouse_ev);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(ProfileChooserView::IsShowing());
}

#if defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_IOS)
// This test doesn't make sense for ChromeOS since it has a different
// multi-profiles menu in the system tray instead.
//
// Mobile platforms are also excluded due to a lack of avatar menu.
#define MAYBE_ViewProfileUMA DISABLED_ViewProfileUMA
#else
#define MAYBE_ViewProfileUMA ViewProfileUMA
#endif

// TODO(mlerman): Re-enable the test to MAYBE_ViewProfileUMA once there is a
// launch plan for EnableAccountConsistency.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewBrowserTest, DISABLED_ViewProfileUMA) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kProfileAvatarTutorialShown, 0);

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView());
}
