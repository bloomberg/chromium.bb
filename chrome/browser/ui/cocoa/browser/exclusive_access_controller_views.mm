// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/exclusive_access_controller_views.h"

#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/exclusive_access_bubble_window_controller.h"
#include "chrome/browser/ui/status_bubble.h"

ExclusiveAccessController::ExclusiveAccessController(
    BrowserWindowController* controller,
    Browser* browser)
    : controller_(controller),
      browser_(browser),
      bubble_type_(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) {}

ExclusiveAccessController::~ExclusiveAccessController() {}

void ExclusiveAccessController::Show() {
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
  [cocoa_bubble_ closeImmediately];
  cocoa_bubble_.reset();
  url_ = GURL();
  bubble_type_ = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

void ExclusiveAccessController::Layout(CGFloat max_y) {
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
  // TODO(mgiuca): Route this signal to the exclusive access bubble on Mac.
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

BrowserWindow* ExclusiveAccessController::GetBrowserWindow() const {
  return [controller_ browserWindow];
}
