// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_utils.h"

using content::MessageLoopRunner;
using content::NavigationController;
using content::WebContents;

class ManagedModeResourceThrottleTest : public InProcessBrowserTest {
 protected:
  ManagedModeResourceThrottleTest() : managed_user_service_(NULL) {}
  virtual ~ManagedModeResourceThrottleTest() {}

 private:
  virtual void SetUpOnMainThread() OVERRIDE;

  ManagedUserService* managed_user_service_;
};

void ManagedModeResourceThrottleTest::SetUpOnMainThread() {
  Profile* profile = browser()->profile();
  managed_user_service_ = ManagedUserServiceFactory::GetForProfile(profile);
  profile->GetPrefs()->SetBoolean(prefs::kProfileIsManaged, true);
  managed_user_service_->Init();
}

// Tests that blocking a request for a WebContents without a
// ManagedModeNavigationObserver doesn't crash.
IN_PROC_BROWSER_TEST_F(ManagedModeResourceThrottleTest, NoNavigationObserver) {
  scoped_ptr<WebContents> web_contents(
      WebContents::Create(WebContents::CreateParams(browser()->profile())));
  NavigationController& controller = web_contents->GetController();
  content::TestNavigationObserver observer(
      content::Source<NavigationController>(&controller), 1);
  controller.LoadURL(GURL("http://www.example.com"), content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  observer.Wait();
}
