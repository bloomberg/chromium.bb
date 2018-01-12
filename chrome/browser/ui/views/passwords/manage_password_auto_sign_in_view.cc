// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_auto_sign_in_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

int ManagePasswordAutoSignInView::auto_signin_toast_timeout_ = 3;

ManagePasswordAutoSignInView::~ManagePasswordAutoSignInView() = default;

ManagePasswordAutoSignInView::ManagePasswordAutoSignInView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent), observed_browser_(this) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  const autofill::PasswordForm& form = parent_->model()->pending_password();
  base::string16 upper_text, lower_text = form.username_value;
  if (ChromeLayoutProvider::Get()->IsHarmonyMode()) {
    upper_text =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE_MD);
  } else {
    lower_text = l10n_util::GetStringFUTF16(
        IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE, lower_text);
  }
  CredentialsItemView* credential = new CredentialsItemView(
      this, upper_text, lower_text, kButtonHoverColor, &form,
      content::BrowserContext::GetDefaultStoragePartition(
          parent_->model()->GetProfile())
          ->GetURLLoaderFactoryForBrowserProcess());
  credential->SetEnabled(false);
  AddChildView(credential);

  // Setup the observer and maybe start the timer.
  Browser* browser =
      chrome::FindBrowserWithWebContents(parent_->GetWebContents());
  DCHECK(browser);

// Sign-in dialogs opened for inactive browser windows do not auto-close on
// MacOS. This matches existing Cocoa bubble behavior.
// TODO(varkha): Remove the limitation as part of http://crbug/671916 .
#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  observed_browser_.Add(browser_view->GetWidget());
#endif
  if (browser->window()->IsActive())
    timer_.Start(FROM_HERE, GetTimeout(), this,
                 &ManagePasswordAutoSignInView::OnTimer);
}

void ManagePasswordAutoSignInView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  NOTREACHED();
}

void ManagePasswordAutoSignInView::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (active && !timer_.IsRunning()) {
    timer_.Start(FROM_HERE, GetTimeout(), this,
                 &ManagePasswordAutoSignInView::OnTimer);
  }
}

void ManagePasswordAutoSignInView::OnWidgetClosing(views::Widget* widget) {
  observed_browser_.RemoveAll();
}

void ManagePasswordAutoSignInView::OnTimer() {
  parent_->model()->OnAutoSignInToastTimeout();
  parent_->CloseBubble();
}

base::TimeDelta ManagePasswordAutoSignInView::GetTimeout() {
  return base::TimeDelta::FromSeconds(
      ManagePasswordAutoSignInView::auto_signin_toast_timeout_);
}
