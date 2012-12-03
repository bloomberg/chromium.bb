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
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

ConstrainedWindowMac2::ConstrainedWindowMac2(
    ConstrainedWindowMacDelegate2* delegate,
    content::WebContents* web_contents,
    NSWindow* window)
    : delegate_(delegate),
      web_contents_(web_contents),
      window_([window retain]),
      pending_show_(false) {
  DCHECK(web_contents);
  DCHECK(window_.get());
  ConstrainedWindowTabHelper* constrained_window_tab_helper =
      ConstrainedWindowTabHelper::FromWebContents(web_contents);
  constrained_window_tab_helper->AddConstrainedDialog(this);

  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                 content::Source<content::WebContents>(web_contents));
}

ConstrainedWindowMac2::~ConstrainedWindowMac2() {
}

void ConstrainedWindowMac2::ShowConstrainedWindow() {
  NSWindow* parent_window = GetParentWindow();
  NSView* parent_view = GetSheetParentViewForWebContents(web_contents_);
  if (!parent_window || !parent_view) {
    pending_show_ = true;
    return;
  }

  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:parent_window];
  [controller showSheet:window_ forParentView:parent_view];
}

void ConstrainedWindowMac2::CloseConstrainedWindow() {
  // This function may be called even if the constrained window was never shown.
  // Unset |pending_show_| to prevent the window from being reshown.
  pending_show_ = false;

  [[ConstrainedWindowSheetController controllerForSheet:window_]
      closeSheet:window_];
  ConstrainedWindowTabHelper* constrained_window_tab_helper =
      ConstrainedWindowTabHelper::FromWebContents(web_contents_);
  constrained_window_tab_helper->WillClose(this);
  if (delegate_)
    delegate_->OnConstrainedWindowClosed(this);
}

void ConstrainedWindowMac2::PulseConstrainedWindow() {
  [[ConstrainedWindowSheetController controllerForSheet:window_]
      pulseSheet:window_];
}

gfx::NativeWindow ConstrainedWindowMac2::GetNativeWindow() {
  return window_;
}

bool ConstrainedWindowMac2::CanShowConstrainedWindow() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser)
    return true;
  return !browser->window()->IsInstantTabShowing();
}

void ConstrainedWindowMac2::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED) {
    NOTREACHED();
    return;
  }

  if (pending_show_) {
    pending_show_ = false;
    ShowConstrainedWindow();
  }
}

NSWindow* ConstrainedWindowMac2::GetParentWindow() const {
  // Tab contents in a tabbed browser may not be inside a window. For this
  // reason use a browser window if possible.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (browser)
    return browser->window()->GetNativeWindow();

  return web_contents_->GetView()->GetTopLevelNativeWindow();
}
