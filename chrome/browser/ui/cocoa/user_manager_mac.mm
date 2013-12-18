// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/user_manager_mac.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

// Default window size. Taken from the views implementation in
// chrome/browser/ui/views/user_manager_view.cc.
// TODO(noms): Figure out if this size can be computed dynamically or adjusted
// for smaller screens.
const int kWindowWidth = 900;
const int kWindowHeight = 700;

namespace chrome {

// Declared in browser_dialogs.h so others don't have to depend on this header.
void ShowUserManager(const base::FilePath& profile_path_to_focus) {
  UserManagerMac::Show(profile_path_to_focus);
}

void HideUserManager() {
  UserManagerMac::Hide();
}

}  // namespace chrome

// Window controller for the User Manager view.
@interface UserManagerWindowController : NSWindowController <NSWindowDelegate> {
 @private
  scoped_ptr<content::WebContents> webContents_;
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

  // Center the window on the primary screen.
  CGFloat screenHeight =
      [[[NSScreen screens] objectAtIndex:0] frame].size.height;
  CGFloat screenWidth =
      [[[NSScreen screens] objectAtIndex:0] frame].size.width;

  NSRect contentRect = NSMakeRect((screenWidth - kWindowWidth) / 2,
                                  (screenHeight - kWindowHeight) / 2,
                                  kWindowWidth, kWindowHeight);
  NSWindow* window = [[NSWindow alloc]
      initWithContentRect:contentRect
                styleMask:NSTitledWindowMask |
                          NSClosableWindowMask |
                          NSResizableWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [window setTitle:l10n_util::GetNSString(IDS_USER_MANAGER_SCREEN_TITLE)];

  if ((self = [super initWithWindow:window])) {
    userManagerObserver_ = userManagerObserver;

    // Initialize the web view.
    webContents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile)));
    window.contentView = webContents_->GetView()->GetNativeView();
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
                                        content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
  [self show];
}

- (void)show {
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

// static
UserManagerMac* UserManagerMac::instance_ = NULL;

UserManagerMac::UserManagerMac(Profile* profile) {
  window_controller_.reset([[UserManagerWindowController alloc]
      initWithProfile:profile withObserver:this]);
}

UserManagerMac::~UserManagerMac() {
}

// static
void UserManagerMac::Show(const base::FilePath& profile_path_to_focus) {
  if (instance_) {
    [instance_->window_controller_ show];
    return;
  }

  // Create the guest profile, if necessary, and open the User Manager
  // from the guest profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->CreateProfileAsync(
      ProfileManager::GetGuestProfilePath(),
      base::Bind(&UserManagerMac::OnGuestProfileCreated,
                 profile_path_to_focus),
      base::string16(),
      base::string16(),
      std::string());
}

// static
void UserManagerMac::Hide() {
  if (instance_)
    [instance_->window_controller_ close];
}

// static
bool UserManagerMac::IsShowing() {
  return instance_ ? [instance_->window_controller_ isVisible]: false;
}

void UserManagerMac::WindowWasClosed() {
  instance_ = NULL;
  delete this;
}

void UserManagerMac::OnGuestProfileCreated(
    const base::FilePath& profile_path_to_focus,
    Profile* guest_profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  instance_ = new UserManagerMac(guest_profile);

  // Tell the webui which user pod should be focused.
  std::string page = chrome::kChromeUIUserManagerURL;

  if (!profile_path_to_focus.empty()) {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile_path_to_focus);
    if (index != std::string::npos) {
      page += "#";
      page += base::IntToString(index);
    }
  }
  [instance_->window_controller_ showURL:GURL(page)];
}

