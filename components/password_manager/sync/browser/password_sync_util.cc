// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/sync/browser/password_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/origin.h"

#if !defined(OS_IOS)
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#endif  // !OS_IOS

using autofill::PasswordForm;
using url::Origin;

namespace {

constexpr char kGoogleChangePasswordSignonRealm[] =
    "https://myaccount.google.com/";

}  // namespace

namespace password_manager {
namespace sync_util {

std::string GetSyncUsernameIfSyncingPasswords(
    const syncer::SyncService* sync_service,
    const SigninManagerBase* signin_manager) {
  if (!signin_manager)
    return std::string();

  // If sync is set up, return early if we aren't syncing passwords.
  if (sync_service &&
      !sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS)) {
    return std::string();
  }

  return signin_manager->GetAuthenticatedAccountInfo().email;
}

bool IsGoogleSyncAccount(const autofill::PasswordForm& form,
                         const syncer::SyncService* sync_service,
                         const SigninManagerBase* signin_manager) {
  const Origin gaia_origin =
      Origin::Create(GaiaUrls::GetInstance()->gaia_url().GetOrigin());
  if (!Origin::Create(GURL(form.signon_realm)).IsSameOriginWith(gaia_origin) &&
      form.signon_realm != kGoogleChangePasswordSignonRealm) {
    return false;
  }

  // The empty username can mean that Chrome did not detect it correctly. For
  // reasons described in http://crbug.com/636292#c1, the username is suspected
  // to be the sync username unless proven otherwise.
  if (form.username_value.empty())
    return true;

  return gaia::AreEmailsSame(
      base::UTF16ToUTF8(form.username_value),
      GetSyncUsernameIfSyncingPasswords(sync_service, signin_manager));
}

bool IsSyncAccountCredential(const autofill::PasswordForm& form,
                             const syncer::SyncService* sync_service,
                             const SigninManagerBase* signin_manager,
                             PrefService* prefs) {
#if defined(OS_IOS)
  return IsGoogleSyncAccount(form, sync_service, signin_manager);
#else
  if (safe_browsing::MatchesPasswordProtectionLoginURL(form.origin, *prefs) ||
      safe_browsing::MatchesPasswordProtectionChangePasswordURL(form.origin,
                                                                *prefs)) {
    // Form is on one of the enterprise configured password protection URLs,
    // then we need to check its user name field.
    std::string sync_user_name =
        GetSyncUsernameIfSyncingPasswords(sync_service, signin_manager);

    // User is not signed in or is not syncing password.
    if (sync_user_name.empty())
      return false;

    // For some SSO case, username might not be the complete email address.
    // It might be the email prefix before '@'.
    std::string username = base::UTF16ToUTF8(form.username_value);
    if (username.find('@') == std::string::npos) {
      username += "@";
      username += gaia::ExtractDomainName(sync_user_name);
    }
    return gaia::AreEmailsSame(username, sync_user_name);
  }
  return IsGoogleSyncAccount(form, sync_service, signin_manager);
#endif
}

}  // namespace sync_util
}  // namespace password_manager
