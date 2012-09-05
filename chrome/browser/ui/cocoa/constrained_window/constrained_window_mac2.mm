// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac2.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#include "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

ConstrainedWindowMac2::ConstrainedWindowMac2(
    TabContents* tab_contents,
    NSWindow* window)
    : tab_contents_(tab_contents),
      window_([window retain]) {
  DCHECK(tab_contents);
  DCHECK(window_.get());
  tab_contents->constrained_window_tab_helper()->AddConstrainedDialog(this);
}

void ConstrainedWindowMac2::ShowConstrainedWindow() {
  NSWindow* parent_window = GetParentWindow();
  DCHECK(parent_window);
  NSView* parent_view = GetSheetParentViewForTabContents(tab_contents_);
  DCHECK(parent_view);

  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:parent_window];
  [controller showSheet:window_ forParentView:parent_view];
}

void ConstrainedWindowMac2::CloseConstrainedWindow() {
  [[ConstrainedWindowSheetController controllerForSheet:window_]
      closeSheet:window_];
  tab_contents_->constrained_window_tab_helper()->WillClose(this);
  delete this;
}

gfx::NativeWindow ConstrainedWindowMac2::GetNativeWindow() {
  return window_;
}

bool ConstrainedWindowMac2::CanShowConstrainedWindow() {
  Browser* browser =
      browser::FindBrowserWithWebContents(tab_contents_->web_contents());
  if (!browser)
    return true;
  return !browser->window()->IsInstantTabShowing();
}

ConstrainedWindowMac2::~ConstrainedWindowMac2() {
}

NSWindow* ConstrainedWindowMac2::GetParentWindow() const {
  // Tab contents in a tabbed browser may not be inside a window. For this
  // reason use a browser window if possible.
  Browser* browser =
      browser::FindBrowserWithWebContents(tab_contents_->web_contents());
  if (browser)
    return browser->window()->GetNativeWindow();

  return tab_contents_->web_contents()->GetView()->GetTopLevelNativeWindow();
}
