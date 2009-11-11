// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_status_ui_helper.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

typedef GoogleServiceAuthError AuthError;

// Given an authentication state, this helper function returns the appropriate
// status message and, if necessary, the text that should appear in the
// re-login link.
static void GetLabelsForAuthError(const AuthError& auth_error,
    ProfileSyncService* service, string16* status_label,
    string16* link_label) {
  if (link_label)
    link_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL));
  if (auth_error.state() == AuthError::INVALID_GAIA_CREDENTIALS) {
    // If the user name is empty then the first login failed, otherwise the
    // credentials are out-of-date.
    if (service->GetAuthenticatedUsername().empty())
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_INVALID_USER_CREDENTIALS));
    else
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_INFO_OUT_OF_DATE));
  } else if (auth_error.state() == AuthError::CONNECTION_FAILED) {
    // Note that there is little the user can do if the server is not
    // reachable. Since attempting to re-connect is done automatically by
    // the Syncer, we do not show the (re)login link.
    status_label->assign(
        l10n_util::GetStringFUTF16(IDS_SYNC_SERVER_IS_UNREACHABLE,
                              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
    if (link_label)
      link_label->clear();
  } else {
    status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_ERROR_SIGNING_IN));
  }
}

// Returns the message that should be displayed when the user is authenticated
// and can connect to the sync server. If the user hasn't yet authenticated, an
// empty string is returned.
static string16 GetSyncedStateStatusLabel(ProfileSyncService* service) {
  string16 label;
  string16 user_name(service->GetAuthenticatedUsername());
  if (user_name.empty())
    return label;

  return l10n_util::GetStringFUTF16(
      IDS_SYNC_ACCOUNT_SYNCED_TO_USER_WITH_TIME,
      user_name,
      WideToUTF16(service->GetLastSyncedTimeString()));
}

// static
SyncStatusUIHelper::MessageType SyncStatusUIHelper::GetLabels(
    ProfileSyncService* service, string16* status_label,
    string16* link_label) {
  MessageType result_type(SYNCED);

  if (!service) {
    return PRE_SYNCED;
  }

  if (service->HasSyncSetupCompleted()) {
    ProfileSyncService::Status status(service->QueryDetailedSyncStatus());
    const AuthError& auth_error = service->GetAuthError();

    // Either show auth error information with a link to re-login, auth in prog,
    // or note that everything is OK with the last synced time.
    if (status.authenticated) {
      // Everything is peachy.
      status_label->assign(GetSyncedStateStatusLabel(service));
      DCHECK_EQ(auth_error.state(), AuthError::NONE);
    } else if (service->UIShouldDepictAuthInProgress()) {
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
      result_type = PRE_SYNCED;
    } else if (auth_error.state() != AuthError::NONE) {
      GetLabelsForAuthError(auth_error, service, status_label, link_label);
      result_type = SYNC_ERROR;
    }
  } else {
    // Either show auth error information with a link to re-login, auth in prog,
    // or provide a link to continue with setup.
    result_type = PRE_SYNCED;
    if (service->SetupInProgress()) {
      ProfileSyncService::Status status(service->QueryDetailedSyncStatus());
      const AuthError& auth_error = service->GetAuthError();
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS));
      if (service->UIShouldDepictAuthInProgress()) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
      } else if (auth_error.state() != AuthError::NONE) {
        status_label->clear();
        GetLabelsForAuthError(auth_error, service, status_label, NULL);
        result_type = SYNC_ERROR;
      } else if (!status.authenticated) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_ACCOUNT_DETAILS_NOT_ENTERED));
      }
    } else if (service->unrecoverable_error_detected()) {
      result_type = SYNC_ERROR;
      status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_SETUP_ERROR));
    } else {
      status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_NOT_SET_UP_INFO));
    }
  }
  return result_type;
}
