// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "chrome/browser/sync/sync_status_ui_helper.h"

#include "base/string_util.h"
#include "chrome/browser/sync/auth_error_state.h"
#include "chrome/browser/sync/personalization_strings.h"
#include "chrome/browser/sync/profile_sync_service.h"

// Given an authentication state, this helper function returns the appropriate
// status message and, if necessary, the text that should appear in the
// re-login link.
static void GetLabelsForAuthError(AuthErrorState auth_error,
    ProfileSyncService* service, std::wstring* status_label,
    std::wstring* link_label) {
  if (link_label)
    link_label->assign(kSyncReLoginLinkLabel);
  if (auth_error == AUTH_ERROR_INVALID_GAIA_CREDENTIALS) {
    // If the user name is empty then the first login failed, otherwise the
    // credentials are out-of-date.
    if (service->GetAuthenticatedUsername().empty())
      status_label->append(kSyncInvalidCredentialsError);
    else
      status_label->append(kSyncExpiredCredentialsError);
  } else if (auth_error == AUTH_ERROR_CONNECTION_FAILED) {
    // Note that there is little the user can do if the server is not
    // reachable. Since attempting to re-connect is done automatically by
    // the Syncer, we do not show the (re)login link.
    status_label->append(kSyncServerUnavailableMsg);
    if (link_label)
      link_label->clear();
  } else {
    status_label->append(kSyncOtherLoginErrorLabel);
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

  label += kSyncAccountLabel;
  label += user_name;
  label += L"\n";
  label += kLastSyncedLabel;
  label += service->GetLastSyncedTimeString();
  return label;
}

// static
SyncStatusUIHelper::MessageType SyncStatusUIHelper::GetLabels(
    ProfileSyncService* service, std::wstring* status_label,
    std::wstring* link_label) {
  MessageType result_type(SYNCED);
  bool sync_enabled = service->IsSyncEnabledByUser();

  if (sync_enabled) {
    ProfileSyncService::Status status(service->QueryDetailedSyncStatus());
    AuthErrorState auth_error(service->GetAuthErrorState());

    // Either show auth error information with a link to re-login, auth in prog,
    // or note that everything is OK with the last synced time.
    if (status.authenticated) {
      // Everything is peachy.
      status_label->assign(GetSyncedStateStatusLabel(service));
      DCHECK_EQ(auth_error, AUTH_ERROR_NONE);
    } else if (service->UIShouldDepictAuthInProgress()) {
      status_label->assign(kSyncAuthenticatingLabel);
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
      status_label->assign(UTF8ToWide(kSettingUpText));
      if (service->UIShouldDepictAuthInProgress()) {
        status_label->assign(kSyncAuthenticatingLabel);
      } else if (auth_error != AUTH_ERROR_NONE) {
        status_label->clear();
        GetLabelsForAuthError(auth_error, service, status_label, NULL);
        result_type = SYNC_ERROR;
      } else if (!status.authenticated) {
        status_label->assign(kSyncCredentialsNeededLabel);
      }
    } else {
      status_label->assign(kSyncNotSetupInfo);
    }
  }
  return result_type;
}

#endif  // CHROME_PERSONALIZATION
