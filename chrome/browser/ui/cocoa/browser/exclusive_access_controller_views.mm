// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/exclusive_access_controller_views.h"

#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/exclusive_access_bubble_window_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#import "ui/gfx/mac/coordinate_conversion.h"

ExclusiveAccessController::ExclusiveAccessController(
    BrowserWindowController* controller,
    Browser* browser)
    : controller_(controller),
      browser_(browser),
      bubble_type_(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) {}

ExclusiveAccessController::~ExclusiveAccessController() {}

void ExclusiveAccessController::Show() {
  if (ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()) {
    views_bubble_.reset(
        new ExclusiveAccessBubbleViews(this, url_, bubble_type_));
    return;
  }

  [cocoa_bubble_ closeImmediately];
  cocoa_bubble_.reset([[ExclusiveAccessBubbleWindowController alloc]
                 initWithOwner:controller_
      exclusive_access_manager:browser_->exclusive_access_manager()
                       profile:browser_->profile()
                           url:url_
                    bubbleType:bubble_type_]);
  [cocoa_bubble_ showWindow];
}

void ExclusiveAccessController::Destroy() {
  views_bubble_.reset();
  [cocoa_bubble_ closeImmediately];
  cocoa_bubble_.reset();
  url_ = GURL();
  bubble_type_ = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

void ExclusiveAccessController::Layout(CGFloat max_y) {
  if (views_bubble_)
    views_bubble_->RepositionIfVisible();
  [cocoa_bubble_ positionInWindowAtTop:max_y];
}

Profile* ExclusiveAccessController::GetProfile() {
  return browser_->profile();
}

bool ExclusiveAccessController::IsFullscreen() const {
  return [controller_ isInAnyFullscreenMode];
}

bool ExclusiveAccessController::SupportsFullscreenWithToolbar() const {
  return chrome::mac::SupportsSystemFullscreen();
}

void ExclusiveAccessController::UpdateFullscreenWithToolbar(bool with_toolbar) {
  [controller_ updateFullscreenWithToolbar:with_toolbar];
}

void ExclusiveAccessController::ToggleFullscreenToolbar() {
  [controller_ toggleFullscreenToolbar];
}

bool ExclusiveAccessController::IsFullscreenWithToolbar() const {
  return IsFullscreen() && ![controller_ inPresentationMode];
}

// See the Fullscreen terminology section and the (Fullscreen) interface
// category in browser_window_controller.h for a detailed explanation of the
// logic in this method.
void ExclusiveAccessController::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    bool with_toolbar) {
  url_ = url;
  bubble_type_ = bubble_type;
  if (browser_->exclusive_access_manager()
          ->fullscreen_controller()
          ->IsWindowFullscreenForTabOrPending())
    [controller_ enterWebContentFullscreen];
  else if (!url.is_empty())
    [controller_ enterExtensionFullscreen];
  else
    [controller_ enterBrowserFullscreenWithToolbar:with_toolbar];
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
    ui::Accelerator* accelerator) {
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
  NSPoint location = [window convertScreenToBase:[NSEvent mouseLocation]];
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
