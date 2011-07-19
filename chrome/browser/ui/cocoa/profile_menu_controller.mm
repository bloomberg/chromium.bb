// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profile_menu_controller.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "chrome/browser/ui/profile_menu_model.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface ProfileMenuController (Private)
- (void)activeBrowserChangedTo:(Browser*)browser;
@end

namespace ProfileMenuControllerInternal {

class Observer : public BrowserList::Observer {
 public:
  Observer(ProfileMenuController* controller) : controller_(controller) {
    BrowserList::AddObserver(this);
  }

  ~Observer() {
    BrowserList::RemoveObserver(this);
  }

  // BrowserList::Observer:
  virtual void OnBrowserAdded(const Browser* browser) {}
  virtual void OnBrowserRemoved(const Browser* browser) {}
  virtual void OnBrowserSetLastActive(const Browser* browser) {
    [controller_ activeBrowserChangedTo:const_cast<Browser*>(browser)];
  }

 private:
  ProfileMenuController* controller_;  // Weak; owns this.
};

}  // namespace ProfileMenuControllerInternal

////////////////////////////////////////////////////////////////////////////////

@implementation ProfileMenuController

- (id)initWithMainMenuItem:(NSMenuItem*)item {
  if ((self = [super init])) {
    mainMenuItem_ = item;
    observer_.reset(new ProfileMenuControllerInternal::Observer(self));
  }
  return self;
}

// Private /////////////////////////////////////////////////////////////////////

// Notifies the controller that the active browser has changed and that the
// menu item and menu need to be updated to reflect that.
- (void)activeBrowserChangedTo:(Browser*)browser {
  submenuModel_.reset(new ProfileMenuModel(browser));
  submenuController_.reset(
      [[MenuController alloc] initWithModel:submenuModel_.get()
                     useWithPopUpButtonCell:NO]);

  [mainMenuItem_ setSubmenu:[submenuController_ menu]];

  NSMenu* submenu = [mainMenuItem_ submenu];
  if (browser) {
    [submenu setTitle:
        base::SysUTF8ToNSString(browser->profile()->GetProfileName())];
  }

  if (![[submenu title] length])
    [submenu setTitle:l10n_util::GetNSString(IDS_PROFILES_OPTIONS_GROUP_NAME)];
}

@end
