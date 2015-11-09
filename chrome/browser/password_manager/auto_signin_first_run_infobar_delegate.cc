// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/auto_signin_first_run_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/android/infobars/auto_signin_first_run_infobar.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/infobars/core/infobar.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
void AutoSigninFirstRunInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const base::string16& message) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockBrandingEnabled(
          ProfileSyncServiceFactory::GetForProfile(profile));
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(make_scoped_ptr(new AutoSigninFirstRunInfoBar(
          make_scoped_ptr(new AutoSigninFirstRunInfoBarDelegate(
              web_contents, is_smartlock_branding_enabled, message)))));
}

AutoSigninFirstRunInfoBarDelegate::AutoSigninFirstRunInfoBarDelegate(
    content::WebContents* web_contents,
    bool is_smartlock_branding_enabled,
    const base::string16& message)
    : PasswordManagerInfoBarDelegate(), web_contents_(web_contents) {
  gfx::Range explanation_link_range;
  GetAutoSigninPromptFirstRunExperienceExplanation(
      is_smartlock_branding_enabled, &explanation_, &explanation_link_range);
  SetMessageLinkRange(explanation_link_range);
  SetMessage(message);
}

AutoSigninFirstRunInfoBarDelegate::~AutoSigninFirstRunInfoBarDelegate() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      profile->GetPrefs());
}

base::string16 AutoSigninFirstRunInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON);
}

bool AutoSigninFirstRunInfoBarDelegate::Accept() {
  return true;
}

bool AutoSigninFirstRunInfoBarDelegate::Cancel() {
  return true;
}
