// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_dialogs_views.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/hung_renderer_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/browser/ui/views/validation_message_bubble_view.h"
#include "content/public/browser/web_contents.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"
#endif

// static
void TabDialogs::CreateForWebContents(content::WebContents* contents) {
  DCHECK(contents);
  if (!FromWebContents(contents))
    contents->SetUserData(UserDataKey(), new TabDialogsViews(contents));
}

TabDialogsViews::TabDialogsViews(content::WebContents* contents)
    : web_contents_(contents) {
  DCHECK(contents);
}

TabDialogsViews::~TabDialogsViews() {
}

gfx::NativeView TabDialogsViews::GetDialogParentView() const {
  return web_contents_->GetNativeView();
}

void TabDialogsViews::ShowCollectedCookies() {
  // Deletes itself on close.
  new CollectedCookiesViews(web_contents_);
}

void TabDialogsViews::ShowHungRendererDialog(
    const content::WebContentsUnresponsiveState& unresponsive_state) {
  HungRendererDialogView::Show(web_contents_, unresponsive_state);
}

void TabDialogsViews::HideHungRendererDialog() {
  HungRendererDialogView::Hide(web_contents_);
}

void TabDialogsViews::ShowProfileSigninConfirmation(
    Browser* browser,
    Profile* profile,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate) {
#if !defined(OS_CHROMEOS)
  ProfileSigninConfirmationDialogViews::ShowDialog(browser, profile, username,
                                                   std::move(delegate));
#else
  NOTREACHED();
#endif
}

void TabDialogsViews::ShowManagePasswordsBubble(bool user_action) {
  if (ManagePasswordsBubbleView::manage_password_bubble()) {
    // The bubble is currently shown for some other tab. We should close it now
    // and open for |web_contents_|.
    ManagePasswordsBubbleView::CloseCurrentBubble();
  }
  ManagePasswordsBubbleView::ShowBubble(
      web_contents_, user_action ? ManagePasswordsBubbleView::USER_GESTURE
                                 : ManagePasswordsBubbleView::AUTOMATIC);
}

void TabDialogsViews::HideManagePasswordsBubble() {
  if (!ManagePasswordsBubbleView::manage_password_bubble())
    return;
  content::WebContents* bubble_web_contents =
      ManagePasswordsBubbleView::manage_password_bubble()->web_contents();
  if (web_contents_ == bubble_web_contents)
    ManagePasswordsBubbleView::CloseCurrentBubble();
}

base::WeakPtr<ValidationMessageBubble> TabDialogsViews::ShowValidationMessage(
    const gfx::Rect& anchor_in_root_view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  return (new ValidationMessageBubbleView(
      web_contents_, anchor_in_root_view, main_text, sub_text))->AsWeakPtr();
}
