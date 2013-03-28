// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"

namespace {
// Maximum width of a username - we trim emails that are wider than this so
// the wrench menu doesn't get ridiculously wide.
const int kUsernameMaxWidth = 200;
}  // namespace

namespace signin_ui_util {

GlobalError* GetSignedInServiceError(Profile* profile) {
  // Auth errors have the highest priority - after that, individual service
  // errors.
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile);
  SigninGlobalError* signin_error = signin_manager->signin_global_error();
  if (signin_error && signin_error->HasBadge())
    return signin_error;

  // No auth error - now try other services. Currently the list is just hard-
  // coded but in the future if we add more we can create some kind of
  // registration framework.
  if (profile->IsSyncAccessible()) {
    ProfileSyncService* service =
        ProfileSyncServiceFactory::GetForProfile(profile);
    SyncGlobalError* error = service->sync_global_error();
    if (error && error->HasBadge())
      return error;
  }
  return NULL;
}

string16 GetSigninMenuLabel(Profile* profile) {
  GlobalError* error = signin_ui_util::GetSignedInServiceError(profile);
  if (error)
    return error->MenuItemLabel();

  // No errors, so just display the signed in user, if any.
  ProfileSyncService* service = profile->IsSyncAccessible() ?
      ProfileSyncServiceFactory::GetForProfile(profile) : NULL;

  // Even if the user is signed in, don't display the "signed in as..."
  // label if we're still setting up sync.
  if (!service || !service->FirstSetupInProgress()) {
    std::string username;
    SigninManager* signin_manager =
        SigninManagerFactory::GetForProfileIfExists(profile);
    if (signin_manager)
      username = signin_manager->GetAuthenticatedUsername();
    if (!username.empty() && !signin_manager->AuthInProgress()) {
      string16 elided_username = ui::ElideEmail(UTF8ToUTF16(username),
                                                gfx::Font(),
                                                kUsernameMaxWidth);
      return l10n_util::GetStringFUTF16(IDS_SYNC_MENU_SYNCED_LABEL,
                                        elided_username);
    }
  }
  return l10n_util::GetStringFUTF16(IDS_SYNC_MENU_PRE_SYNCED_LABEL,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
}

// Given an authentication state this helper function returns various labels
// that can be used to display information about the state.
void GetStatusLabelsForAuthError(const SigninManager& signin_manager,
                                 string16* status_label,
                                 string16* link_label) {
  string16 username = UTF8ToUTF16(signin_manager.GetAuthenticatedUsername());
  string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  if (link_label)
    link_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL));

  switch (signin_manager.signin_global_error()->GetLastAuthError().state()) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      // If the user name is empty then the first login failed, otherwise the
      // credentials are out-of-date.
      if (username.empty()) {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_INVALID_USER_CREDENTIALS));
        }
      } else {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_INFO_OUT_OF_DATE));
        }
      }
      break;
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE));
      }
      if (link_label)
        link_label->clear();
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      // Note that there is little the user can do if the server is not
      // reachable. Since attempting to re-connect is done automatically by
      // the Syncer, we do not show the (re)login link.
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringFUTF16(IDS_SYNC_SERVER_IS_UNREACHABLE,
                                       product_name));
      }
      break;
    default:
      if (status_label) {
        status_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_ERROR_SIGNING_IN));
      }
      break;
  }
}


}  // namespace signin_ui_util
