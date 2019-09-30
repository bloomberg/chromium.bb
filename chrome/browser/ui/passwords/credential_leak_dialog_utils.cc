// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;
using password_manager::metrics_util::LeakDialogType;

namespace leak_dialog_utils {
namespace {

// Formats the |origin| to a human-friendly url string.
base::string16 GetFormattedUrl(const GURL& origin) {
  return url_formatter::FormatUrlForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

}  // namespace

base::string16 GetAcceptButtonLabel(CredentialLeakType leak_type) {
  return l10n_util::GetStringUTF16(
      ShouldCheckPasswords(leak_type) ? IDS_LEAK_CHECK_CREDENTIALS : IDS_OK);
}

base::string16 GetCancelButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

base::string16 GetDescription(CredentialLeakType leak_type,
                              const GURL& origin) {
  const base::string16 formatted = GetFormattedUrl(origin);
  if (!ShouldCheckPasswords(leak_type)) {
    std::vector<size_t> offsets;
    base::string16 bold_message = l10n_util::GetStringUTF16(
        IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_BOLD_MESSAGE);
    return l10n_util::GetStringFUTF16(
        IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE, bold_message, formatted,
        &offsets);
  } else if (password_manager::IsPasswordSaved(leak_type)) {
    return l10n_util::GetStringUTF16(
        IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE, formatted);
  }
}

base::string16 GetTitle(CredentialLeakType leak_type) {
  return l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_TITLE);
}

bool ShouldCheckPasswords(CredentialLeakType leak_type) {
  return password_manager::IsPasswordUsedOnOtherSites(leak_type) &&
         password_manager::IsSyncingPasswordsNormally(leak_type);
}

bool ShouldShowCancelButton(CredentialLeakType leak_type) {
  return ShouldCheckPasswords(leak_type);
}

LeakDialogType GetLeakDialogType(CredentialLeakType leak_type) {
  if (!ShouldCheckPasswords(leak_type))
    return LeakDialogType::kChange;

  return password_manager::IsPasswordSaved(leak_type)
             ? LeakDialogType::kCheckup
             : LeakDialogType::kCheckupAndChange;
}

GURL GetPasswordCheckupURL() {
  std::string value = base::GetFieldTrialParamValueByFeature(
      password_manager::features::kLeakDetection, "leak-check-url");
  if (value.empty())
    return GURL(chrome::kPasswordCheckupURL);
  return GURL(value);
}

gfx::Range GetChangePasswordBoldRange(CredentialLeakType leak_type,
                                      const GURL& origin) {
  if (ShouldCheckPasswords(leak_type))
    return gfx::Range();

  std::vector<size_t> offsets;
  const base::string16 bold_message = l10n_util::GetStringUTF16(
      IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_BOLD_MESSAGE);
  const base::string16 change_password_message = l10n_util::GetStringFUTF16(
      IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE, bold_message,
      GetFormattedUrl(origin), &offsets);
  return offsets.empty()
             ? gfx::Range()
             : gfx::Range(offsets[0], offsets[0] + bold_message.length());
}

}  // namespace leak_dialog_utils
