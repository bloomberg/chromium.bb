// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_dialogs_cocoa.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"
#import "chrome/browser/ui/cocoa/hung_renderer_controller.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/profiles/profile_signin_confirmation_dialog_cocoa.h"
#include "chrome/browser/ui/cocoa/tab_dialogs_views_mac.h"
#import "chrome/browser/ui/cocoa/validation_message_bubble_cocoa.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "content/public/browser/web_contents.h"

// static
void TabDialogs::CreateForWebContents(content::WebContents* contents) {
  DCHECK(contents);

  if (!FromWebContents(contents)) {
    std::unique_ptr<TabDialogs> tab_dialogs =
        chrome::ShowAllDialogsWithViewsToolkit()
            ? base::MakeUnique<TabDialogsViewsMac>(contents)
            : base::MakeUnique<TabDialogsCocoa>(contents);
    contents->SetUserData(UserDataKey(), std::move(tab_dialogs));
  }
}

TabDialogsCocoa::TabDialogsCocoa(content::WebContents* contents)
    : web_contents_(contents) {
  DCHECK(contents);
}

TabDialogsCocoa::~TabDialogsCocoa() {
}

gfx::NativeView TabDialogsCocoa::GetDialogParentView() const {
  // View hierarchy of the contents view:
  // NSView  -- switchView, same for all tabs
  // +- TabContentsContainerView  -- TabContentsController's view
  //    +- WebContentsViewCocoa
  //
  // Changing it? Do not forget to modify
  // -[TabStripController swapInTabAtIndex:] too.
  return [web_contents_->GetNativeView() superview];
}

void TabDialogsCocoa::ShowCollectedCookies() {
  // Deletes itself on close.
  new CollectedCookiesMac(web_contents_);
}

void TabDialogsCocoa::ShowHungRendererDialog(
    const content::WebContentsUnresponsiveState& unresponsive_state) {
  [HungRendererController showForWebContents:web_contents_];
}

void TabDialogsCocoa::HideHungRendererDialog() {
  [HungRendererController endForWebContents:web_contents_];
}

void TabDialogsCocoa::ShowProfileSigninConfirmation(
    Browser* browser,
    Profile* profile,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate) {
  ProfileSigninConfirmationDialogCocoa::Show(browser, web_contents_, profile,
                                             username, std::move(delegate));
}

void TabDialogsCocoa::ShowManagePasswordsBubble(bool user_action) {
  ManagePasswordsBubbleCocoa::Show(web_contents_, user_action);
}

void TabDialogsCocoa::HideManagePasswordsBubble() {
  // The bubble is closed when it loses the focus.
}

base::WeakPtr<ValidationMessageBubble> TabDialogsCocoa::ShowValidationMessage(
    const gfx::Rect& anchor_in_root_view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  return (new ValidationMessageBubbleCocoa(
      web_contents_, anchor_in_root_view, main_text, sub_text))->AsWeakPtr();
}
