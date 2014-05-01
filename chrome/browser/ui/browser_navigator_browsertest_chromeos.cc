// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_browsertest.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/chromeos/login/chrome_restart_request.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/web_contents.h"

namespace {

// This is a test implementation of a MultiUserWindowManager which allows to
// test a visiting window on another desktop. It will install and remove itself
// from the system upon creation / destruction.
// The creation function gets a |browser| which is shown on |desktop_owner|'s
// desktop.
class TestMultiUserWindowManager : public chrome::MultiUserWindowManager {
 public:
  TestMultiUserWindowManager(
      Browser* visiting_browser,
      const std::string& desktop_owner);
  virtual ~TestMultiUserWindowManager();

  aura::Window* created_window() { return created_window_; }

  // MultiUserWindowManager overrides:
  virtual void SetWindowOwner(
      aura::Window* window, const std::string& user_id) OVERRIDE;
  virtual const std::string& GetWindowOwner(
      aura::Window* window) const OVERRIDE;
  virtual void ShowWindowForUser(
      aura::Window* window, const std::string& user_id) OVERRIDE;
  virtual bool AreWindowsSharedAmongUsers() const OVERRIDE;
  virtual void GetOwnersOfVisibleWindows(
      std::set<std::string>* user_ids) const OVERRIDE;
  virtual bool IsWindowOnDesktopOfUser(
      aura::Window* window,
      const std::string& user_id) const OVERRIDE;
  virtual const std::string& GetUserPresentingWindow(
      aura::Window* window) const OVERRIDE;
  virtual void AddUser(content::BrowserContext* profile) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

 private:
  // The window of the visiting browser.
  aura::Window* browser_window_;
  // The owner of the visiting browser.
  std::string browser_owner_;
  // The owner of the currently shown desktop.
  std::string desktop_owner_;
  // The created window.
  aura::Window* created_window_;
  // The location of the window.
  std::string created_window_shown_for_;

  DISALLOW_COPY_AND_ASSIGN(TestMultiUserWindowManager);
};

TestMultiUserWindowManager::TestMultiUserWindowManager(
    Browser* visiting_browser,
    const std::string& desktop_owner)
    : browser_window_(visiting_browser->window()->GetNativeWindow()),
      browser_owner_(multi_user_util::GetUserIDFromProfile(
          visiting_browser->profile())),
      desktop_owner_(desktop_owner),
      created_window_(NULL),
      created_window_shown_for_(browser_owner_) {
  // Create a window manager for a visiting user.
  chrome::MultiUserWindowManager::SetInstanceForTest(
      this,
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED);
}

TestMultiUserWindowManager::~TestMultiUserWindowManager() {
  // This object is owned by the MultiUserWindowManager since the
  // SetInstanceForTest call. As such no uninstall is required.
}

// MultiUserWindowManager overrides:
void TestMultiUserWindowManager::SetWindowOwner(
    aura::Window* window, const std::string& user_id) {
  NOTREACHED();
}

const std::string& TestMultiUserWindowManager::GetWindowOwner(
    aura::Window* window) const {
  // No matter which window will get queried - all browsers belong to the
  // original browser's user.
  return browser_owner_;
}

void TestMultiUserWindowManager::ShowWindowForUser(
    aura::Window* window,
    const std::string& user_id) {
  // This class is only able to handle one additional window <-> user
  // association beside the creation parameters.
  // If no association has yet been requested remember it now.
  DCHECK(!created_window_);
  created_window_ = window;
  created_window_shown_for_ = user_id;
}

bool TestMultiUserWindowManager::AreWindowsSharedAmongUsers() const {
  return browser_owner_ != desktop_owner_;
}

void TestMultiUserWindowManager::GetOwnersOfVisibleWindows(
    std::set<std::string>* user_ids) const {
}

bool TestMultiUserWindowManager::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const std::string& user_id) const {
  return GetUserPresentingWindow(window) == user_id;
}

const std::string& TestMultiUserWindowManager::GetUserPresentingWindow(
    aura::Window* window) const {
  if (window == browser_window_)
    return desktop_owner_;
  if (created_window_ && window == created_window_)
    return created_window_shown_for_;
  // We can come here before the window gets registered.
  return browser_owner_;
}

void TestMultiUserWindowManager::AddUser(content::BrowserContext* profile) {
}

void TestMultiUserWindowManager::AddObserver(Observer* observer) {
}

void TestMultiUserWindowManager::RemoveObserver(Observer* observer) {
}

GURL GetGoogleURL() {
  return GURL("http://www.google.com/");
}

// Subclass that tests navigation while in the Guest session.
class BrowserGuestSessionNavigatorTest: public BrowserNavigatorTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    CommandLine command_line_copy = *command_line;
    command_line_copy.AppendSwitchASCII(
        chromeos::switches::kLoginProfile, "user");
    chromeos::GetOffTheRecordCommandLine(GetGoogleURL(),
                                         true,
                                         command_line_copy,
                                         command_line);
  }
};

// This test verifies that the settings page is opened in the incognito window
// in Guest Session (as well as all other windows in Guest session).
IN_PROC_BROWSER_TEST_F(BrowserGuestSessionNavigatorTest,
                       Disposition_Settings_UseIncognitoWindow) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());

  // Navigate to the settings page.
  chrome::NavigateParams params(MakeNavigateParams(incognito_browser));
  params.disposition = SINGLETON_TAB;
  params.url = GURL("chrome://chrome/settings");
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&params);

  // Settings page should be opened in incognito window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_EQ(incognito_browser, params.browser);
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
  EXPECT_EQ(GURL("chrome://chrome/settings"),
            incognito_browser->tab_strip_model()->GetActiveWebContents()->
                GetURL());
}

// Test that in multi user environments a newly created browser gets created
// on the same desktop as the browser is shown on.
IN_PROC_BROWSER_TEST_F(BrowserGuestSessionNavigatorTest,
                       Browser_Gets_Created_On_Visiting_Desktop) {
  // Test 1: Test that a browser created from a visiting browser will be on the
  // same visiting desktop.
  {
    const std::string desktop_user_id = "desktop_user_id@fake.com";
    TestMultiUserWindowManager* manager =
        new TestMultiUserWindowManager(browser(), desktop_user_id);

    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

    // Navigate to the settings page.
    chrome::NavigateParams params(MakeNavigateParams(browser()));
    params.disposition = NEW_POPUP;
    params.url = GURL("chrome://chrome/settings");
    params.window_action = chrome::NavigateParams::SHOW_WINDOW;
    params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
    params.browser = browser();
    chrome::Navigate(&params);

    EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

    aura::Window* created_window = manager->created_window();
    ASSERT_TRUE(created_window);
    EXPECT_TRUE(manager->IsWindowOnDesktopOfUser(created_window,
                                                 desktop_user_id));
  }
  // Test 2: Test that a window which is not visiting does not cause an owner
  // assignment of a newly created browser.
  {
    std::string browser_owner =
        multi_user_util::GetUserIDFromProfile(browser()->profile());
    TestMultiUserWindowManager* manager =
        new TestMultiUserWindowManager(browser(), browser_owner);

    // Navigate to the settings page.
    chrome::NavigateParams params(MakeNavigateParams(browser()));
    params.disposition = NEW_POPUP;
    params.url = GURL("chrome://chrome/settings");
    params.window_action = chrome::NavigateParams::SHOW_WINDOW;
    params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
    params.browser = browser();
    chrome::Navigate(&params);

    EXPECT_EQ(3u, chrome::GetTotalBrowserCount());

    // The ShowWindowForUser should not have been called since the window is
    // already on the correct desktop.
    ASSERT_FALSE(manager->created_window());
  }
}

}  // namespace
