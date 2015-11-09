// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/generated_password_saved_infobar_delegate_android.h"

#include <cstddef>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/chrome_application.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

GeneratedPasswordSavedInfoBarDelegateAndroid::
    ~GeneratedPasswordSavedInfoBarDelegateAndroid() {}

void GeneratedPasswordSavedInfoBarDelegateAndroid::OnInlineLinkClicked() {
  if (smart_lock_branding_enabled_) {
    InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
        content::OpenURLParams(
            GURL(chrome::kPasswordManagerAccountDashboardURL),
            content::Referrer(), NEW_FOREGROUND_TAB,
            ui::PAGE_TRANSITION_LINK, false));
  } else {
    chrome::android::ChromeApplication::ShowPasswordSettings();
  }
}

GeneratedPasswordSavedInfoBarDelegateAndroid::
    GeneratedPasswordSavedInfoBarDelegateAndroid(
        content::WebContents *web_contents)
    : button_label_(l10n_util::GetStringUTF16(IDS_OK)),
      web_contents_(web_contents),
      smart_lock_branding_enabled_(
          password_bubble_experiment::IsSmartLockBrandingEnabled(
              ProfileSyncServiceFactory::GetForProfile(
                  Profile::FromBrowserContext(
                      web_contents->GetBrowserContext())))) {
  base::string16 link = l10n_util::GetStringUTF16(
      IDS_MANAGE_PASSWORDS_LINK);
  int confirmation_id = IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT_INFOBAR;
  if (smart_lock_branding_enabled_) {
    std::string management_hostname =
        GURL(chrome::kPasswordManagerAccountDashboardURL).host();
    link = base::UTF8ToUTF16(management_hostname);
    confirmation_id =
        IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_SMART_LOCK_TEXT_INFOBAR;
  }

  size_t offset;
  message_text_ = l10n_util::GetStringFUTF16(confirmation_id, link, &offset);
  inline_link_range_ = gfx::Range(offset, offset + link.length());
}

infobars::InfoBarDelegate::Type
GeneratedPasswordSavedInfoBarDelegateAndroid::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int GeneratedPasswordSavedInfoBarDelegateAndroid::GetIconId() const {
  return IDR_INFOBAR_SAVE_PASSWORD;
}
