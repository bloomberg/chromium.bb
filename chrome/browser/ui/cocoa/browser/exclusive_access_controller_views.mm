// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/exclusive_access_controller_views.h"

#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/new_back_shortcut_bubble.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/gfx/mac/coordinate_conversion.h"

ExclusiveAccessController::ExclusiveAccessController(
    BrowserWindowController* controller,
    Browser* browser)
    : controller_(controller),
      browser_(browser),
      bubble_type_(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) {
  pref_registrar_.Init(GetProfile()->GetPrefs());
  pref_registrar_.Add(
      prefs::kShowFullscreenToolbar,
      base::Bind(&ExclusiveAccessController::UpdateFullscreenToolbar,
                 base::Unretained(this)));
}

ExclusiveAccessController::~ExclusiveAccessController() {}

void ExclusiveAccessController::Show() {
  // Hide the backspace shortcut bubble, to avoid overlapping.
  new_back_shortcut_bubble_.reset();

  views_bubble_.reset(new ExclusiveAccessBubbleViews(this, url_, bubble_type_));
}

void ExclusiveAccessController::MaybeShowNewBackShortcutBubble(bool forward) {
  if (!new_back_shortcut_bubble_ || !new_back_shortcut_bubble_->IsVisible()) {
      // Show the bubble at most five times.
      PrefService* prefs = GetProfile()->GetPrefs();
      int shown_count = prefs->GetInteger(prefs::kBackShortcutBubbleShownCount);
      constexpr int kMaxShownCount = 5;
      if (shown_count >= kMaxShownCount)
        return;

      // Only show the bubble when the user presses a shortcut twice within
      // three seconds.
      const base::TimeTicks now = base::TimeTicks::Now();
      constexpr base::TimeDelta kRepeatWindow = base::TimeDelta::FromSeconds(3);
      if (last_back_shortcut_press_time_.is_null() ||
          ((now - last_back_shortcut_press_time_) > kRepeatWindow)) {
        last_back_shortcut_press_time_ = now;
        return;
      }

      // Hide the exclusive access bubble, to avoid overlapping.
      views_bubble_.reset();

      new_back_shortcut_bubble_.reset(new NewBackShortcutBubble(this));
      prefs->SetInteger(prefs::kBackShortcutBubbleShownCount, shown_count + 1);
      last_back_shortcut_press_time_ = base::TimeTicks();
  }

  new_back_shortcut_bubble_->UpdateContent(forward);
}

void ExclusiveAccessController::HideNewBackShortcutBubble() {
  if (new_back_shortcut_bubble_)
    new_back_shortcut_bubble_->Hide();
}

void ExclusiveAccessController::Destroy() {
  views_bubble_.reset();
  url_ = GURL();
  bubble_type_ = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

Profile* ExclusiveAccessController::GetProfile() {
  return browser_->profile();
}

bool ExclusiveAccessController::IsFullscreen() const {
  return [controller_ isInAnyFullscreenMode];
}

void ExclusiveAccessController::UpdateUIForTabFullscreen(
    TabFullscreenState state) {
  [controller_ updateUIForTabFullscreen:state];
}

void ExclusiveAccessController::UpdateFullscreenToolbar() {
  [[controller_ fullscreenToolbarController]
      updateToolbarStyleExitingTabFullscreen:NO];
}

// See the Fullscreen terminology section and the (Fullscreen) interface
// category in browser_window_controller.h for a detailed explanation of the
// logic in this method.
void ExclusiveAccessController::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {
  url_ = url;
  bubble_type_ = bubble_type;
  if (browser_->exclusive_access_manager()
          ->fullscreen_controller()
          ->IsWindowFullscreenForTabOrPending())
    [controller_ enterWebContentFullscreen];
  else
    [controller_ enterBrowserFullscreen];
}

void ExclusiveAccessController::ExitFullscreen() {
  [controller_ exitAnyFullscreen];
}

void ExclusiveAccessController::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {
  url_ = url;
  bubble_type_ = bubble_type;
  [controller_ updateFullscreenExitBubble];
}

void ExclusiveAccessController::OnExclusiveAccessUserInput() {
  if (views_bubble_)
    views_bubble_->OnUserInput();
}

content::WebContents* ExclusiveAccessController::GetActiveWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void ExclusiveAccessController::UnhideDownloadShelf() {
  GetBrowserWindow()->GetDownloadShelf()->Unhide();
}

void ExclusiveAccessController::HideDownloadShelf() {
  GetBrowserWindow()->GetDownloadShelf()->Hide();
  StatusBubble* statusBubble = GetBrowserWindow()->GetStatusBubble();
  if (statusBubble)
    statusBubble->Hide();
}

bool ExclusiveAccessController::GetAcceleratorForCommandId(
    int cmd_id,
    ui::Accelerator* accelerator) const {
  *accelerator =
      *AcceleratorsCocoa::GetInstance()->GetAcceleratorForCommand(cmd_id);
  return true;
}

ExclusiveAccessManager* ExclusiveAccessController::GetExclusiveAccessManager() {
  return browser_->exclusive_access_manager();
}

views::Widget* ExclusiveAccessController::GetBubbleAssociatedWidget() {
  NOTREACHED();  // Only used for non-simplified UI.
  return nullptr;
}

ui::AcceleratorProvider* ExclusiveAccessController::GetAcceleratorProvider() {
  return this;
}

gfx::NativeView ExclusiveAccessController::GetBubbleParentView() const {
  return [[controller_ window] contentView];
}

gfx::Point ExclusiveAccessController::GetCursorPointInParent() const {
  NSWindow* window = [controller_ window];
  NSPoint location =
      ui::ConvertPointFromScreenToWindow(window, [NSEvent mouseLocation]);
  return gfx::Point(location.x,
                    NSHeight([[window contentView] frame]) - location.y);
}

gfx::Rect ExclusiveAccessController::GetClientAreaBoundsInScreen() const {
  return gfx::ScreenRectFromNSRect([[controller_ window] frame]);
}

bool ExclusiveAccessController::IsImmersiveModeEnabled() {
  return false;
}

gfx::Rect ExclusiveAccessController::GetTopContainerBoundsInScreen() {
  NOTREACHED();  // Only used for ImmersiveMode.
  return gfx::Rect();
}

BrowserWindow* ExclusiveAccessController::GetBrowserWindow() const {
  return [controller_ browserWindow];
}
