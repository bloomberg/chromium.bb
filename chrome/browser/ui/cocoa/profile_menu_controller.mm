// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profile_menu_controller.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_interface.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"

@interface ProfileMenuController (Private)
- (void)initializeMenu;
@end

namespace ProfileMenuControllerInternal {

class Observer : public chrome::BrowserListObserver,
                 public AvatarMenuModelObserver {
 public:
  Observer(ProfileMenuController* controller) : controller_(controller) {
    BrowserList::AddObserver(this);
  }

  ~Observer() {
    BrowserList::RemoveObserver(this);
  }

  // chrome::BrowserListObserver:
  virtual void OnBrowserAdded(Browser* browser) {}
  virtual void OnBrowserRemoved(Browser* browser) {
    [controller_ activeBrowserChangedTo:chrome::GetLastActiveBrowser()];
  }
  virtual void OnBrowserSetLastActive(Browser* browser) {
    [controller_ activeBrowserChangedTo:browser];
  }

  // AvatarMenuModelObserver:
  virtual void OnAvatarMenuModelChanged(AvatarMenuModel* model) {
    [controller_ rebuildMenu];
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

    scoped_nsobject<NSMenu> menu(
        [[NSMenu alloc] initWithTitle:
            l10n_util::GetNSStringWithFixup(IDS_PROFILES_OPTIONS_GROUP_NAME)]);
    [mainMenuItem_ setSubmenu:menu];

    // This object will be constructed as part of nib loading, which happens
    // before the message loop starts and g_browser_process is available.
    // Schedule this on the loop to do work when the browser is ready.
    [self performSelector:@selector(initializeMenu)
               withObject:nil
               afterDelay:0];
  }
  return self;
}

- (IBAction)switchToProfileFromMenu:(id)sender {
  model_->SwitchToProfile([sender tag], false);
  ProfileMetrics::LogProfileSwitchUser(ProfileMetrics::SWITCH_PROFILE_MENU);
}

- (IBAction)switchToProfileFromDock:(id)sender {
  // Explicitly bring to the foreground when taking action from the dock.
  [NSApp activateIgnoringOtherApps:YES];
  model_->SwitchToProfile([sender tag], false);
  ProfileMetrics::LogProfileSwitchUser(ProfileMetrics::SWITCH_PROFILE_DOCK);
}

- (IBAction)editProfile:(id)sender {
  model_->EditProfile(model_->GetActiveProfileIndex());
}

- (IBAction)newProfile:(id)sender {
  model_->AddNewProfile(ProfileMetrics::ADD_NEW_USER_MENU);
}

- (BOOL)insertItemsIntoMenu:(NSMenu*)menu
                   atOffset:(NSInteger)offset
                   fromDock:(BOOL)dock {
  if (!model_->ShouldShowAvatarMenu())
    return NO;

  if (dock) {
    NSString* headerName =
        l10n_util::GetNSStringWithFixup(IDS_PROFILES_OPTIONS_GROUP_NAME);
    scoped_nsobject<NSMenuItem> header(
        [[NSMenuItem alloc] initWithTitle:headerName
                                   action:NULL
                            keyEquivalent:@""]);
    [header setEnabled:NO];
    [menu insertItem:header atIndex:offset++];
  }

  for (size_t i = 0; i < model_->GetNumberOfItems(); ++i) {
    const AvatarMenuModel::Item& itemData = model_->GetItemAt(i);
    NSString* name = base::SysUTF16ToNSString(itemData.name);
    SEL action = dock ? @selector(switchToProfileFromDock:)
                      : @selector(switchToProfileFromMenu:);
    NSMenuItem* item = [self createItemWithTitle:name
                                          action:action];
    [item setTag:itemData.model_index];
    if (dock) {
      [item setIndentationLevel:1];
    } else {
      [item setImage:itemData.icon.ToNSImage()];
      [item setState:itemData.active ? NSOnState : NSOffState];
    }
    [menu insertItem:item atIndex:i + offset];
  }

  return YES;
}

// Private /////////////////////////////////////////////////////////////////////

- (NSMenu*)menu {
  return [mainMenuItem_ submenu];
}

- (void)initializeMenu {
  observer_.reset(new ProfileMenuControllerInternal::Observer(self));
  model_.reset(new AvatarMenuModel(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      observer_.get(),
      NULL));

  [[self menu] addItem:[NSMenuItem separatorItem]];

  NSMenuItem* item =
      [self createItemWithTitle:l10n_util::GetNSStringWithFixup(
                                    IDS_PROFILES_CUSTOMIZE_PROFILE)
                         action:@selector(editProfile:)];
  [[self menu] addItem:item];

  [[self menu] addItem:[NSMenuItem separatorItem]];
  item = [self createItemWithTitle:l10n_util::GetNSStringWithFixup(
                                       IDS_PROFILES_CREATE_NEW_PROFILE_OPTION)
                            action:@selector(newProfile:)];
  [[self menu] addItem:item];

  [self rebuildMenu];
}

// Notifies the controller that the active browser has changed and that the
// menu item and menu need to be updated to reflect that.
- (void)activeBrowserChangedTo:(Browser*)browser {
  // Tell the model that the browser has changed.
  model_->set_browser(browser);

  // If |browser| is NULL, it may be because the current profile was deleted
  // and there are no other loaded profiles. In this case, calling
  // |model_->GetActiveProfileIndex()| may result in a profile being loaded,
  // which is inappropriate to do on the UI thread.
  //
  // An early return provides the desired behavior:
  //   a) If the profile was deleted, the menu would have been rebuilt and no
  //      profile will have a check mark.
  //   b) If the profile was not deleted, but there is no active browser, then
  //      the previous profile will remain checked.
  if (!browser)
    return;

  size_t active_profile_index = model_->GetActiveProfileIndex();

  // Update the state for the menu items.
  for (size_t i = 0; i < model_->GetNumberOfItems(); ++i) {
    size_t tag = model_->GetItemAt(i).model_index;
    [[[self menu] itemWithTag:tag]
        setState:active_profile_index == tag ? NSOnState
                                             : NSOffState];
  }
}

- (void)rebuildMenu {
  NSMenu* menu = [self menu];

  for (NSMenuItem* item = [menu itemAtIndex:0];
       ![item isSeparatorItem];
       item = [menu itemAtIndex:0]) {
    [menu removeItemAtIndex:0];
  }

  BOOL hasContent = [self insertItemsIntoMenu:menu atOffset:0 fromDock:NO];

  [mainMenuItem_ setHidden:!hasContent];
}

- (NSMenuItem*)createItemWithTitle:(NSString*)title action:(SEL)sel {
  scoped_nsobject<NSMenuItem> item(
      [[NSMenuItem alloc] initWithTitle:title action:sel keyEquivalent:@""]);
  [item setTarget:self];
  return [item.release() autorelease];
}

@end
