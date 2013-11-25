// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using autofill::PasswordFormMap;

ManagePasswordsBubbleModel::ManagePasswordsBubbleModel(
    WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);

  password_submitted_ = content_settings->password_submitted();
  if (password_submitted_) {
    if (content_settings->password_to_be_saved())
      manage_passwords_bubble_state_ = PASSWORD_TO_BE_SAVED;
    else
      manage_passwords_bubble_state_ = MANAGE_PASSWORDS_AFTER_SAVING;
  } else {
    manage_passwords_bubble_state_ = MANAGE_PASSWORDS;
  }

  title_ = l10n_util::GetStringUTF16(
      (manage_passwords_bubble_state_ == PASSWORD_TO_BE_SAVED) ?
          IDS_SAVE_PASSWORD : IDS_MANAGE_PASSWORDS);
  if (password_submitted_)
    pending_credentials_ = content_settings->pending_credentials();
  best_matches_ = content_settings->best_matches();
  manage_link_ =
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS_LINK);
}

ManagePasswordsBubbleModel::~ManagePasswordsBubbleModel() {}

void ManagePasswordsBubbleModel::OnCancelClicked() {
  manage_passwords_bubble_state_ = PASSWORD_TO_BE_SAVED;
}

void ManagePasswordsBubbleModel::OnSaveClicked() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);
  content_settings->SavePassword();
  content_settings->unset_password_to_be_saved();
  manage_passwords_bubble_state_ = MANAGE_PASSWORDS_AFTER_SAVING;
}

void ManagePasswordsBubbleModel::OnManageLinkClicked() {
  chrome::ShowSettingsSubPage(chrome::FindBrowserWithWebContents(web_contents_),
                              chrome::kPasswordManagerSubPage);
}

void ManagePasswordsBubbleModel::PasswordAction(
    autofill::PasswordForm password_form, bool remove) {
  if (!web_contents_)
    return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PasswordStore* password_store = PasswordStoreFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS).get();
  DCHECK(password_store);
  if (remove)
    password_store->RemoveLogin(password_form);
  else
    password_store->AddLogin(password_form);
  // This is necessary in case the bubble is instantiated again, we thus do not
  // display the pending credentials if they were deleted.
  if (password_form.username_value == pending_credentials_.username_value) {
    TabSpecificContentSettings* content_settings =
        TabSpecificContentSettings::FromWebContents(web_contents_);
    content_settings->password_submitted(!remove);
  }
}

void ManagePasswordsBubbleModel::DeleteFromBestMatches(
    autofill::PasswordForm password_form) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);
  content_settings->remove_from_best_matches(password_form);
}

void ManagePasswordsBubbleModel::WebContentsDestroyed(
    content::WebContents* web_contents) {
  // The WebContents have been destroyed.
  web_contents_ = NULL;
}
