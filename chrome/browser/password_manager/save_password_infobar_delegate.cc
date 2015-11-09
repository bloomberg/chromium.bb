// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/save_password_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/infobars/core/infobar.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int GetCancelButtonText(password_manager::CredentialSourceType source_type) {
  return IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON;
}

}  // namespace

// static
void SavePasswordInfoBarDelegate::Create(
    content::WebContents* web_contents,
    scoped_ptr<password_manager::PasswordFormManager> form_to_save,
    const std::string& uma_histogram_suffix,
    password_manager::CredentialSourceType source_type) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  sync_driver::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockBrandingEnabled(sync_service);
  bool should_show_first_run_experience =
      password_bubble_experiment::ShouldShowSavePromptFirstRunExperience(
          sync_service, profile->GetPrefs());
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(CreateSavePasswordInfoBar(
          make_scoped_ptr(new SavePasswordInfoBarDelegate(
              web_contents, form_to_save.Pass(), uma_histogram_suffix,
              source_type, is_smartlock_branding_enabled,
              should_show_first_run_experience))));
}

SavePasswordInfoBarDelegate::~SavePasswordInfoBarDelegate() {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.InfoBarResponse",
                            infobar_response_,
                            password_manager::metrics_util::NUM_RESPONSE_TYPES);

  password_manager::metrics_util::LogUIDismissalReason(infobar_response_);

  // The shortest period for which the prompt needs to live, so that we don't
  // consider it killed prematurely, as might happen, e.g., if a pre-rendered
  // page gets swapped in (and the current WebContents is destroyed).
  const base::TimeDelta kMinimumPromptDisplayTime =
      base::TimeDelta::FromSeconds(1);

  if (!uma_histogram_suffix_.empty()) {
    password_manager::metrics_util::LogUMAHistogramEnumeration(
        "PasswordManager.SavePasswordPromptResponse_" + uma_histogram_suffix_,
        infobar_response_,
        password_manager::metrics_util::NUM_RESPONSE_TYPES);
    password_manager::metrics_util::LogUMAHistogramBoolean(
        "PasswordManager.SavePasswordPromptDisappearedQuickly_" +
            uma_histogram_suffix_,
        timer_.Elapsed() < kMinimumPromptDisplayTime);
  }
  if (should_show_first_run_experience_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    password_bubble_experiment::RecordSavePromptFirstRunExperienceWasShown(
        profile->GetPrefs());
  }
}

SavePasswordInfoBarDelegate::SavePasswordInfoBarDelegate(
    content::WebContents* web_contents,
    scoped_ptr<password_manager::PasswordFormManager> form_to_save,
    const std::string& uma_histogram_suffix,
    password_manager::CredentialSourceType source_type,
    bool is_smartlock_branding_enabled,
    bool should_show_first_run_experience)
    : PasswordManagerInfoBarDelegate(),
      form_to_save_(form_to_save.Pass()),
      infobar_response_(password_manager::metrics_util::NO_RESPONSE),
      uma_histogram_suffix_(uma_histogram_suffix),
      source_type_(source_type),
      should_show_first_run_experience_(should_show_first_run_experience),
      web_contents_(web_contents) {
  if (!uma_histogram_suffix_.empty()) {
    password_manager::metrics_util::LogUMAHistogramBoolean(
        "PasswordManager.SavePasswordPromptDisplayed_" + uma_histogram_suffix_,
        true);
  }
  base::string16 message;
  gfx::Range message_link_range = gfx::Range();
  GetSavePasswordDialogTitleTextAndLinkRange(
      web_contents->GetVisibleURL(), form_to_save_->observed_form().origin,
      is_smartlock_branding_enabled, false, &message, &message_link_range);
  SetMessage(message);
  SetMessageLinkRange(message_link_range);
}

base::string16 SavePasswordInfoBarDelegate::GetFirstRunExperienceMessage() {
  return should_show_first_run_experience_
             ? l10n_util::GetStringUTF16(
                   IDS_PASSWORD_MANAGER_SAVE_PROMPT_FIRST_RUN_EXPERIENCE)
             : base::string16();
}

void SavePasswordInfoBarDelegate::InfoBarDismissed() {
  DCHECK(form_to_save_.get());
  infobar_response_ = password_manager::metrics_util::INFOBAR_DISMISSED;
}

base::string16 SavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_PASSWORD_MANAGER_SAVE_BUTTON
                                       : GetCancelButtonText(source_type_));
}

bool SavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save_.get());
  form_to_save_->Save();
  infobar_response_ = password_manager::metrics_util::REMEMBER_PASSWORD;
  return true;
}

bool SavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save_.get());
  form_to_save_->PermanentlyBlacklist();
  infobar_response_ = password_manager::metrics_util::NEVER_REMEMBER_PASSWORD;
  return true;
}
