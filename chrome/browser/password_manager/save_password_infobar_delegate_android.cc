// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/save_password_infobar_delegate_android.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

// static
void SavePasswordInfoBarDelegate::Create(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManager> form_to_save) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockUser(sync_service);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new SavePasswordInfoBarDelegate(web_contents, std::move(form_to_save),
                                          is_smartlock_branding_enabled))));
}

SavePasswordInfoBarDelegate::~SavePasswordInfoBarDelegate() {
  password_manager::metrics_util::LogSaveUIDismissalReason(infobar_response_);
  form_to_save_->metrics_recorder()->RecordUIDismissalReason(infobar_response_);
}

SavePasswordInfoBarDelegate::SavePasswordInfoBarDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManager> form_to_save,
    bool is_smartlock_branding_enabled)
    : PasswordManagerInfoBarDelegate(),
      form_to_save_(std::move(form_to_save)),
      infobar_response_(password_manager::metrics_util::NO_DIRECT_INTERACTION) {
  base::string16 message;
  gfx::Range message_link_range = gfx::Range();
  PasswordTitleType type =
      form_to_save_->pending_credentials().federation_origin.unique()
          ? PasswordTitleType::SAVE_PASSWORD
          : PasswordTitleType::SAVE_ACCOUNT;
  GetSavePasswordDialogTitleTextAndLinkRange(
      web_contents->GetVisibleURL(), form_to_save_->observed_form().origin,
      is_smartlock_branding_enabled, type,
      &message, &message_link_range);
  SetMessage(message);
  SetMessageLinkRange(message_link_range);

  form_to_save_->metrics_recorder()->RecordPasswordBubbleShown(
      form_to_save_->GetCredentialSource(),
      password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
}

infobars::InfoBarDelegate::InfoBarIdentifier
SavePasswordInfoBarDelegate::GetIdentifier() const {
  return SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE;
}

void SavePasswordInfoBarDelegate::InfoBarDismissed() {
  DCHECK(form_to_save_.get());
  infobar_response_ = password_manager::metrics_util::CLICKED_CANCEL;
}

base::string16 SavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_PASSWORD_MANAGER_SAVE_BUTTON
                                       : IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
}

bool SavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save_.get());
  form_to_save_->Save();
  infobar_response_ = password_manager::metrics_util::CLICKED_SAVE;
  return true;
}

bool SavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save_.get());
  form_to_save_->PermanentlyBlacklist();
  infobar_response_ = password_manager::metrics_util::CLICKED_NEVER;
  return true;
}
