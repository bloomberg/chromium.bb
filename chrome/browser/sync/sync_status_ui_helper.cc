// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(BROWSER_SYNC)

#include "chrome/browser/sync/sync_status_ui_helper.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/sync/auth_error_state.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

// Given an authentication state, this helper function returns the appropriate
// status message and, if necessary, the text that should appear in the
// re-login link.
static void GetLabelsForAuthError(AuthErrorState auth_error,
    ProfileSyncService* service, std::wstring* status_label,
    std::wstring* link_label) {
  if (link_label)
    link_label->assign(l10n_util::GetString(IDS_SYNC_RELOGIN_LINK_LABEL));
  if (auth_error == AUTH_ERROR_INVALID_GAIA_CREDENTIALS) {
    // If the user name is empty then the first login failed, otherwise the
    // credentials are out-of-date.
    if (service->GetAuthenticatedUsername().empty())
      status_label->assign(
          l10n_util::GetString(IDS_SYNC_INVALID_USER_CREDENTIALS));
    else
      status_label->assign(
          l10n_util::GetString(IDS_SYNC_LOGIN_INFO_OUT_OF_DATE));
  } else if (auth_error == AUTH_ERROR_CONNECTION_FAILED) {
    // Note that there is little the user can do if the server is not
    // reachable. Since attempting to re-connect is done automatically by
    // the Syncer, we do not show the (re)login link.
    status_label->assign(
        l10n_util::GetStringF(IDS_SYNC_SERVER_IS_UNREACHABLE,
                              l10n_util::GetString(IDS_PRODUCT_NAME)));
    if (link_label)
      link_label->clear();
  } else {
    status_label->assign(l10n_util::GetString(IDS_SYNC_ERROR_SIGNING_IN));
  }
}

// Returns the message that should be displayed when the user is authenticated
// and can connect to the sync server. If the user hasn't yet authenticated, an
// empty string is returned.
static std::wstring GetSyncedStateStatusLabel(ProfileSyncService* service) {
  std::wstring label;
  std::wstring user_name(UTF16ToWide(service->GetAuthenticatedUsername()));
  if (user_name.empty())
    return label;

  return l10n_util::GetStringF(IDS_SYNC_ACCOUNT_SYNCED_TO_USER_WITH_TIME,
                               user_name, service->GetLastSyncedTimeString());
}

// static
SyncStatusUIHelper::MessageType SyncStatusUIHelper::GetLabels(
    ProfileSyncService* service, std::wstring* status_label,
    std::wstring* link_label) {
  MessageType result_type(SYNCED);

  if (!service) {
    return PRE_SYNCED;
  }

  if (service->HasSyncSetupCompleted()) {
    ProfileSyncService::Status status(service->QueryDetailedSyncStatus());
    AuthErrorState auth_error(service->GetAuthErrorState());

    // Either show auth error information with a link to re-login, auth in prog,
    // or note that everything is OK with the last synced time.
    if (status.authenticated) {
      // Everything is peachy.
      status_label->assign(GetSyncedStateStatusLabel(service));
      DCHECK_EQ(auth_error, AUTH_ERROR_NONE);
    } else if (service->UIShouldDepictAuthInProgress()) {
      status_label->assign(l10n_util::GetString(IDS_SYNC_AUTHENTICATING_LABEL));
      result_type = PRE_SYNCED;
    } else if (auth_error != AUTH_ERROR_NONE) {
      GetLabelsForAuthError(auth_error, service, status_label, link_label);
      result_type = SYNC_ERROR;
    }
  } else {
    // Either show auth error information with a link to re-login, auth in prog,
    // or provide a link to continue with setup.
    result_type = PRE_SYNCED;
    if (service->SetupInProgress()) {
      ProfileSyncService::Status status(service->QueryDetailedSyncStatus());
      AuthErrorState auth_error(service->GetAuthErrorState());
      status_label->assign(
          l10n_util::GetString(IDS_SYNC_NTP_SETUP_IN_PROGRESS));
      if (service->UIShouldDepictAuthInProgress()) {
        status_label->assign(
            l10n_util::GetString(IDS_SYNC_AUTHENTICATING_LABEL));
      } else if (auth_error != AUTH_ERROR_NONE) {
        status_label->clear();
        GetLabelsForAuthError(auth_error, service, status_label, NULL);
        result_type = SYNC_ERROR;
      } else if (!status.authenticated) {
        status_label->assign(
            l10n_util::GetString(IDS_SYNC_ACCOUNT_DETAILS_NOT_ENTERED));
      }
    } else if (service->unrecoverable_error_detected()) {
      result_type = SYNC_ERROR;
      status_label->assign(l10n_util::GetString(IDS_SYNC_SETUP_ERROR));
    } else {
      status_label->assign(l10n_util::GetString(IDS_SYNC_NOT_SET_UP_INFO));
    }
  }
  return result_type;
}

#endif  // defined(BROWSER_SYNC)
