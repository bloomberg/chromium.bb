// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"

#include "base/mac/foundation_util.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_dialogs.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/events/keycodes/keyboard_codes.h"


// An open User Manager window. There can only be one open at a time. This
// is reset to NULL when the window is closed.
UserManagerMac* instance_ = NULL;  // Weak.

// Custom WebContentsDelegate that allows handling of hotkeys.
class UserManagerWebContentsDelegate : public content::WebContentsDelegate {
 public:
  UserManagerWebContentsDelegate() {}

  // WebContentsDelegate implementation. Forwards all unhandled keyboard events
  // to the current window.
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE {
    if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
      return;

    // -getCommandId returns -1 if the event isn't a chrome accelerator.
    int chromeCommandId = [BrowserWindowUtils getCommandId:event];

    // Check for Cmd+A and Cmd+V events that could come from a password field.
    bool isTextEditingCommand =
        (event.modifiers & blink::WebInputEvent::MetaKey) &&
        (event.windowsKeyCode == ui::VKEY_A ||
         event.windowsKeyCode == ui::VKEY_V);

    // Only handle close window Chrome accelerators and text editing ones.
    if (chromeCommandId == IDC_CLOSE_WINDOW || chromeCommandId == IDC_EXIT ||
        isTextEditingCommand) {
      [[NSApp mainMenu] performKeyEquivalent:event.os_event];
    }
  }
};

// Window controller for the User Manager view.
@interface UserManagerWindowController : NSWindowController <NSWindowDelegate> {
 @private
  scoped_ptr<content::WebContents> webContents_;
  scoped_ptr<UserManagerWebContentsDelegate> webContentsDelegate_;
  UserManagerMac* userManagerObserver_;  // Weak.
}
- (void)windowWillClose:(NSNotification*)notification;
- (void)dealloc;
- (id)initWithProfile:(Profile*)profile
         withObserver:(UserManagerMac*)userManagerObserver;
- (void)showURL:(const GURL&)url;
- (void)show;
- (void)close;
- (BOOL)isVisible;
@end

@implementation UserManagerWindowController

- (id)initWithProfile:(Profile*)profile
         withObserver:(UserManagerMac*)userManagerObserver {

  // Center the window on the screen that currently has focus.
  NSScreen* mainScreen = [NSScreen mainScreen];
  CGFloat screenHeight = [mainScreen frame].size.height;
  CGFloat screenWidth = [mainScreen frame].size.width;

  NSRect contentRect =
      NSMakeRect((screenWidth - UserManager::kWindowWidth) / 2,
                 (screenHeight - UserManager::kWindowHeight) / 2,
                 UserManager::kWindowWidth, UserManager::kWindowHeight);
  ChromeEventProcessingWindow* window = [[ChromeEventProcessingWindow alloc]
      initWithContentRect:contentRect
                styleMask:NSTitledWindowMask |
                          NSClosableWindowMask |
                          NSResizableWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO
                   screen:mainScreen];
  [window setTitle:l10n_util::GetNSString(IDS_PRODUCT_NAME)];
  [window setMinSize:NSMakeSize(UserManager::kWindowWidth,
                                UserManager::kWindowHeight)];

  if ((self = [super initWithWindow:window])) {
    userManagerObserver_ = userManagerObserver;

    // Initialize the web view.
    webContents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile)));
    window.contentView = webContents_->GetNativeView();
    webContentsDelegate_.reset(new UserManagerWebContentsDelegate());
    webContents_->SetDelegate(webContentsDelegate_.get());
    DCHECK(window.contentView);

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowWillClose:)
               name:NSWindowWillCloseNotification
             object:self.window];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)showURL:(const GURL&)url {
  webContents_->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
  [self show];
}

- (void)show {
  // Because the User Manager isn't a BrowserWindowController, activating it
  // will not trigger a -windowChangedToProfile and update the menu bar.
  // This is only important if the active profile is Guest, which may have
  // happened after locking a profile.
  Profile* guestProfile = profiles::SetActiveProfileToGuestIfLocked();
  if (guestProfile && guestProfile->IsGuestSession()) {
    AppController* controller =
        base::mac::ObjCCast<AppController>([NSApp delegate]);
    [controller windowChangedToProfile:guestProfile];
  }
  [[self window] makeKeyAndOrderFront:self];
}

- (void)close {
  [[self window] close];
}

-(BOOL)isVisible {
  return [[self window] isVisible];
}

- (void)windowWillClose:(NSNotification*)notification {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  DCHECK(userManagerObserver_);
  userManagerObserver_->WindowWasClosed();
}

@end


void UserManager::Show(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerTutorialMode tutorial_mode,
    profiles::UserManagerProfileSelected profile_open_action) {
  ProfileMetrics::LogProfileSwitchUser(ProfileMetrics::OPEN_USER_MANAGER);
  if (instance_) {
    // If there's a user manager window open already, just activate it.
    [instance_->window_controller() show];
    return;
  }

  // Create the guest profile, if necessary, and open the User Manager
  // from the guest profile.
  profiles::CreateGuestProfileForUserManager(
      profile_path_to_focus,
      tutorial_mode,
      profile_open_action,
      base::Bind(&UserManagerMac::OnGuestProfileCreated));
}

void UserManager::Hide() {
  if (instance_)
    [instance_->window_controller() close];
}

bool UserManager::IsShowing() {
  return instance_ ? [instance_->window_controller() isVisible]: false;
}

UserManagerMac::UserManagerMac(Profile* profile) {
  window_controller_.reset([[UserManagerWindowController alloc]
      initWithProfile:profile withObserver:this]);
}

UserManagerMac::~UserManagerMac() {
}

// static
void UserManagerMac::OnGuestProfileCreated(Profile* guest_profile,
                                           const std::string& url) {
  DCHECK(!instance_);
  instance_ = new UserManagerMac(guest_profile);
  [instance_->window_controller() showURL:GURL(url)];
}

void UserManagerMac::WindowWasClosed() {
  instance_ = NULL;
  delete this;
}
