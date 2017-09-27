// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_dialogs_views_mac.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_controller.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/material_design/material_design_controller.h"
#import "ui/gfx/mac/coordinate_conversion.h"

namespace {

gfx::Point ScreenPointFromBrowser(Browser* browser, NSPoint ns_point) {
  return gfx::ScreenPointFromNSPoint(ui::ConvertPointFromWindowToScreen(
      browser->window()->GetNativeWindow(), ns_point));
}
}

TabDialogsViewsMac::TabDialogsViewsMac(content::WebContents* contents)
    : TabDialogsCocoa(contents) {}

TabDialogsViewsMac::~TabDialogsViewsMac() {}

void TabDialogsViewsMac::ShowCollectedCookies() {
  // Deletes itself on close.
  new CollectedCookiesViews(web_contents());
}

void TabDialogsViewsMac::ShowProfileSigninConfirmation(
    Browser* browser,
    Profile* profile,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate) {
  ProfileSigninConfirmationDialogViews::ShowDialog(browser, profile, username,
                                                   std::move(delegate));
}

void TabDialogsViewsMac::ShowManagePasswordsBubble(bool user_action) {
  if (!ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    TabDialogsCocoa::ShowManagePasswordsBubble(user_action);
    return;
  }
  NSWindow* window = [web_contents()->GetNativeView() window];
  if (!window) {
    // The tab isn't active right now.
    return;
  }

  // Don't show the bubble again if it's already showing. A second click on the
  // location icon in the omnibox will dismiss an open bubble. This behaviour is
  // consistent with the non-Mac views implementation.
  // Note that when the browser is toolkit-views, IsBubbleShown() is checked
  // earlier because the bubble is shown on mouse release (but dismissed on
  // mouse pressed). A Cocoa browser does both on mouse pressed, so a check
  // when showing is sufficient.
  if (ManagePasswordsBubbleView::manage_password_bubble())
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  BrowserWindowController* bwc =
      [BrowserWindowController browserWindowControllerForWindow:window];
  gfx::Point anchor_point =
      ScreenPointFromBrowser(browser, [bwc bookmarkBubblePoint]);
  gfx::NativeView parent =
      platform_util::GetViewForWindow(browser->window()->GetNativeWindow());
  DCHECK(parent);

  LocationBarBubbleDelegateView::DisplayReason reason =
      user_action ? LocationBarBubbleDelegateView::USER_GESTURE
                  : LocationBarBubbleDelegateView::AUTOMATIC;
  ManagePasswordsBubbleView* bubble_view = new ManagePasswordsBubbleView(
      web_contents(), nullptr, anchor_point, reason);
  bubble_view->set_arrow(views::BubbleBorder::TOP_RIGHT);
  bubble_view->set_parent_window(parent);
  views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  bubble_view->ShowForReason(reason);
  KeepBubbleAnchored(bubble_view, GetManagePasswordDecoration(window));
}

void TabDialogsViewsMac::HideManagePasswordsBubble() {
  // Close toolkit-views bubble.
  if (!ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    TabDialogsCocoa::HideManagePasswordsBubble();
    return;
  }
  ManagePasswordsBubbleView::CloseCurrentBubble();
}
