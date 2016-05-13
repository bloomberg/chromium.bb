// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/update_password_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/android/infobars/update_password_infobar.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/infobars/core/infobar.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
void UpdatePasswordInfoBarDelegate::Create(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManager> form_to_save) {
  const bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockBrandingEnabled(
          ProfileSyncServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())));
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(base::WrapUnique(new UpdatePasswordInfoBar(
          base::WrapUnique(new UpdatePasswordInfoBarDelegate(
              web_contents, std::move(form_to_save),
              is_smartlock_branding_enabled)))));
}

UpdatePasswordInfoBarDelegate::~UpdatePasswordInfoBarDelegate() {}

base::string16 UpdatePasswordInfoBarDelegate::GetBranding() const {
  return l10n_util::GetStringUTF16(
      is_smartlock_branding_enabled_
          ? IDS_PASSWORD_MANAGER_SMART_LOCK_FOR_PASSWORDS
          : IDS_PASSWORD_MANAGER_TITLE_BRAND);
}

bool UpdatePasswordInfoBarDelegate::ShowMultipleAccounts() const {
  const password_manager::PasswordFormManager* form_manager =
      passwords_state_.form_manager();
  bool is_password_overriden =
      form_manager && form_manager->password_overridden();
  return GetCurrentForms().size() > 1 && !is_password_overriden;
}

const std::vector<const autofill::PasswordForm*>&
UpdatePasswordInfoBarDelegate::GetCurrentForms() const {
  return passwords_state_.GetCurrentForms();
}

UpdatePasswordInfoBarDelegate::UpdatePasswordInfoBarDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManager> form_to_update,
    bool is_smartlock_branding_enabled)
    : is_smartlock_branding_enabled_(is_smartlock_branding_enabled) {
  // TODO(melandory): Add histograms, crbug.com/577129
  passwords_state_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  passwords_state_.OnUpdatePassword(std::move(form_to_update));
}

infobars::InfoBarDelegate::InfoBarIdentifier
UpdatePasswordInfoBarDelegate::GetIdentifier() const {
  return UPDATE_PASSWORD_INFOBAR_DELEGATE;
}

base::string16 UpdatePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_PASSWORD_MANAGER_UPDATE_BUTTON
                                       : IDS_PASSWORD_MANAGER_CANCEL_BUTTON);
}

bool UpdatePasswordInfoBarDelegate::Accept() {
  UpdatePasswordInfoBar* update_password_infobar =
      static_cast<UpdatePasswordInfoBar*>(infobar());
  password_manager::PasswordFormManager* form_manager =
      passwords_state_.form_manager();
  if (ShowMultipleAccounts()) {
    int form_index = update_password_infobar->GetIdOfSelectedUsername();
    DCHECK_GE(form_index, 0);
    DCHECK_LT(static_cast<size_t>(form_index), GetCurrentForms().size());
    form_manager->Update(*GetCurrentForms()[form_index]);
  } else {
    form_manager->Update(form_manager->pending_credentials());
  }
  return true;
}

bool UpdatePasswordInfoBarDelegate::Cancel() {
  return true;
}
