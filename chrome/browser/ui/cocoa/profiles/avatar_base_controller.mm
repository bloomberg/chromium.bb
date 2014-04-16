// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_base_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_menu_bubble_controller.h"
#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "ui/base/resource/resource_bundle.h"

// Space between the avatar icon and the avatar menu bubble.
const CGFloat kMenuYOffsetAdjust = 1.0;

@interface AvatarBaseController (Private)
// Shows the avatar bubble.
- (IBAction)buttonClicked:(id)sender;
- (void)bubbleWillClose:(NSNotification*)notif;
// Updates the profile name displayed by the avatar button. If |layoutParent| is
// yes, then the BrowserWindowController is notified to relayout the subviews,
// as the button needs to be repositioned.
- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent;
@end

class ProfileInfoUpdateObserver : public ProfileInfoCacheObserver {
 public:
  ProfileInfoUpdateObserver(AvatarBaseController* avatarButton)
      : avatarButton_(avatarButton) {
    g_browser_process->profile_manager()->
        GetProfileInfoCache().AddObserver(this);
  }

  virtual ~ProfileInfoUpdateObserver() {
    g_browser_process->profile_manager()->
        GetProfileInfoCache().RemoveObserver(this);
  }

  // ProfileInfoCacheObserver:
  virtual void OnProfileAdded(const base::FilePath& profile_path) OVERRIDE {
    [avatarButton_ updateAvatarButtonAndLayoutParent:YES];
  }

  virtual void OnProfileWasRemoved(
      const base::FilePath& profile_path,
      const base::string16& profile_name) OVERRIDE {
    [avatarButton_ updateAvatarButtonAndLayoutParent:YES];
  }

  virtual void OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) OVERRIDE {
    [avatarButton_ updateAvatarButtonAndLayoutParent:YES];
  }

  virtual void OnProfileAvatarChanged(
      const base::FilePath& profile_path) OVERRIDE {
    [avatarButton_ updateAvatarButtonAndLayoutParent:YES];
  }

 private:
  AvatarBaseController* avatarButton_;  // Weak; owns this.

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoUpdateObserver);
};

@implementation AvatarBaseController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    browser_ = browser;
    profileInfoObserver_.reset(new ProfileInfoUpdateObserver(self));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:[menuController_ window]];
  [super dealloc];
}

- (NSButton*)buttonView {
  CHECK(button_.get());  // Subclasses must set this.
  return button_.get();
}

- (void)showAvatarBubble:(NSView*)anchor
                withMode:(BrowserWindow::AvatarBubbleMode)mode {
  if (menuController_)
    return;

  DCHECK(chrome::IsCommandEnabled(browser_, IDC_SHOW_AVATAR_MENU));

  NSWindowController* wc =
      [browser_->window()->GetNativeWindow() windowController];
  if ([wc isKindOfClass:[BrowserWindowController class]]) {
    [static_cast<BrowserWindowController*>(wc)
        lockBarVisibilityForOwner:self withAnimation:NO delay:NO];
  }

  // The new avatar bubble does not have an arrow, and it should be anchored
  // to the edge of the avatar button.
  int anchorX = switches::IsNewAvatarMenu() ? NSMaxX([anchor bounds]) :
                                              NSMidX([anchor bounds]);
  NSPoint point = NSMakePoint(anchorX,
                              NSMaxY([anchor bounds]) - kMenuYOffsetAdjust);
  point = [anchor convertPoint:point toView:nil];
  point = [[anchor window] convertBaseToScreen:point];

  // |menuController_| will automatically release itself on close.
  if (switches::IsNewAvatarMenu()) {
    BubbleViewMode viewMode =
        mode == BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT ?
        BUBBLE_VIEW_MODE_PROFILE_CHOOSER :
        BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
    menuController_ =
        [[ProfileChooserController alloc] initWithBrowser:browser_
                                               anchoredAt:point
                                                 withMode:viewMode];
  } else {
    menuController_ =
      [[AvatarMenuBubbleController alloc] initWithBrowser:browser_
                                               anchoredAt:point];
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(bubbleWillClose:)
             name:NSWindowWillCloseNotification
           object:[menuController_ window]];
  [menuController_ showWindow:self];

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
}

- (IBAction)buttonClicked:(id)sender {
  DCHECK_EQ(sender, button_.get());
  [self showAvatarBubble:button_
                withMode:BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT];
}

- (void)bubbleWillClose:(NSNotification*)notif {
  NSWindowController* wc =
      [browser_->window()->GetNativeWindow() windowController];
  if ([wc isKindOfClass:[BrowserWindowController class]]) {
    [static_cast<BrowserWindowController*>(wc)
        releaseBarVisibilityForOwner:self withAnimation:YES delay:NO];
  }
  menuController_ = nil;
}

- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent {
  NOTREACHED();
}

- (BaseBubbleController*)menuController {
  return menuController_;
}

@end
