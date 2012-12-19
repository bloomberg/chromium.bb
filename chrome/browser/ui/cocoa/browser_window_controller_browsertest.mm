// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller.h"

#import "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"
#include "chrome/test/base/in_process_browser_test.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
enum {
  NSWindowDocumentVersionsButton = 6,
  NSWindowFullScreenButton
};
#endif  // MAC_OS_X_VERSION_10_7

typedef InProcessBrowserTest BrowserWindowControllerTest;

void CreateProfileCallback(const base::Closure& quit_closure,
                           Profile* profile,
                           Profile::CreateStatus status) {
  EXPECT_TRUE(profile);
  EXPECT_NE(Profile::CREATE_STATUS_FAIL, status);
  // This will be called multiple times. Wait until the profile is initialized
  // fully to quit the loop.
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    quit_closure.Run();
}

// Tests that adding the first profile moves the Lion fullscreen button over
// correctly.
// DISABLED_ because it regularly times out: http://crbug.com/159002.
IN_PROC_BROWSER_TEST_F(BrowserWindowControllerTest,
                       DISABLED_ProfileAvatarFullscreenButton) {
  if (base::mac::IsOSSnowLeopard())
    return;

  // Initialize the locals.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  NSWindow* window = browser()->window()->GetNativeWindow();
  ASSERT_TRUE(window);

  BrowserWindowController* controller =
      static_cast<BrowserWindowCocoa*>(browser()->window())->cocoa_controller();

  // With only one profile, the fullscreen button should be visible, but the
  // avatar button should not.
  EXPECT_EQ(1u, profile_manager->GetNumberOfProfiles());

  NSButton* fullscreen_button =
      [window standardWindowButton:NSWindowFullScreenButton];
  EXPECT_TRUE(fullscreen_button);
  EXPECT_FALSE([fullscreen_button isHidden]);

  AvatarButtonController* avatar_controller =
      [controller avatarButtonController];
  NSView* avatar = [avatar_controller view];
  EXPECT_TRUE(avatar);
  EXPECT_TRUE([avatar isHidden]);

  // Create a profile asynchronously and run the loop until its creation
  // is complete.
  base::RunLoop run_loop;

  ProfileManager::CreateCallback create_callback =
      base::Bind(&CreateProfileCallback, run_loop.QuitClosure());
  profile_manager->CreateProfileAsync(
      profile_manager->user_data_dir().Append("test"),
      create_callback,
      ASCIIToUTF16("avatar_test"),
      string16(),
      false);

  run_loop.Run();

  // There should now be two profiles, and the avatar button and fullscreen
  // button are both visible.
  EXPECT_EQ(2u, profile_manager->GetNumberOfProfiles());
  EXPECT_FALSE([avatar isHidden]);
  EXPECT_FALSE([fullscreen_button isHidden]);
  EXPECT_EQ([avatar window], [fullscreen_button window]);

  // Make sure the visual order of the buttons is correct and that they don't
  // overlap.
  NSRect avatar_frame = [avatar frame];
  NSRect fullscreen_frame = [fullscreen_button frame];

  EXPECT_LT(NSMinX(fullscreen_frame), NSMinX(avatar_frame));
  EXPECT_LT(NSMaxX(fullscreen_frame), NSMinX(avatar_frame));
}
