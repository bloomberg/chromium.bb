// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace sync_ui_util {

namespace {

// Returns the message that should be displayed when the user is authenticated
// and can connect to the sync server. If the user hasn't yet authenticated, an
// empty string is returned.
base::string16 GetSyncedStateStatusLabel(const syncer::SyncService* service) {
  DCHECK(service);
  if (service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    // User is signed in, but sync is disabled.
    return l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_DISABLED);
  }
  if (service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE)) {
    // User is signed in, but sync has been stopped.
    return l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_SUPPRESSED);
  }
  if (!service->IsSyncFeatureActive()) {
    // User is not signed in, or sync is still initializing.
    return base::string16();
  }

  return l10n_util::GetStringUTF16(
      service->GetUserSettings()->IsSyncEverythingEnabled()
          ? IDS_SYNC_ACCOUNT_SYNCING
          : IDS_SYNC_ACCOUNT_SYNCING_CUSTOM_DATA_TYPES);
}

void GetStatusForActionableError(syncer::ClientAction action,
                                 base::string16* status_label,
                                 base::string16* link_label,
                                 ActionType* action_type) {
  DCHECK(status_label);
  DCHECK(link_label);
  switch (action) {
    case syncer::UPGRADE_CLIENT:
      *status_label = l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT);
      *link_label =
          l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT_LINK_LABEL);
      *action_type = UPGRADE_CLIENT;
      break;
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
      *status_label =
          l10n_util::GetStringUTF16(IDS_SYNC_STATUS_ENABLE_SYNC_ON_ACCOUNT);
      break;
    default:
      *status_label = base::string16();
      break;
  }
}

void GetStatusForUnrecoverableError(bool is_user_signout_allowed,
                                    syncer::ClientAction action,
                                    base::string16* status_label,
                                    base::string16* link_label,
                                    ActionType* action_type) {
  // Unrecoverable error is sometimes accompanied by actionable error.
  // If status message is set display that message, otherwise show generic
  // unrecoverable error message.
  GetStatusForActionableError(action, status_label, link_label, action_type);
  if (status_label->empty()) {
    *action_type = REAUTHENTICATE;
    *link_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL);

#if !defined(OS_CHROMEOS)
    *status_label =
        l10n_util::GetStringUTF16(IDS_SYNC_STATUS_UNRECOVERABLE_ERROR);
    // The message for managed accounts is the same as that of the cros.
    if (!is_user_signout_allowed) {
      *status_label = l10n_util::GetStringUTF16(
          IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT);
    }
#else
    *status_label = l10n_util::GetStringUTF16(
        IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT);
#endif
  }
}

// Depending on the authentication state, returns labels to be used to display
// information about the sync status.
void GetStatusForAuthError(const GoogleServiceAuthError& auth_error,
                           base::string16* status_label,
                           base::string16* link_label,
                           ActionType* action_type) {
  DCHECK(status_label);
  DCHECK(link_label);
  switch (auth_error.state()) {
    case GoogleServiceAuthError::NONE:
      NOTREACHED();
      break;
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      *status_label = l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE);
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      *status_label = l10n_util::GetStringUTF16(IDS_SYNC_SERVER_IS_UNREACHABLE);
      // Note that there is little the user can do if the server is not
      // reachable. Since attempting to re-connect is done automatically by
      // the Syncer, we do not show the (re)login link.
      break;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
    default:
      *status_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_ERROR);
      *link_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL);
      *action_type = REAUTHENTICATE;
      break;
  }
}

// status_label and link_label must either be both null or both non-null.
MessageType GetStatusLabelsImpl(
    const syncer::SyncService* service,
    const identity::IdentityManager* identity_manager,
    bool is_user_signout_allowed,
    const GoogleServiceAuthError& auth_error,
    base::string16* status_label,
    base::string16* link_label,
    ActionType* action_type) {
  DCHECK(service);
  DCHECK_EQ(status_label == nullptr, link_label == nullptr);

  if (!identity_manager->HasPrimaryAccount()) {
    return PRE_SYNCED;
  }

  syncer::SyncStatus status;
  service->QueryDetailedSyncStatus(&status);

  if (service->GetUserSettings()->IsFirstSetupComplete() ||
      service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE)) {
    // The order or priority is going to be: 1. Unrecoverable errors.
    // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.

    if (service->HasUnrecoverableError()) {
      if (status_label && link_label) {
        GetStatusForUnrecoverableError(is_user_signout_allowed,
                                       status.sync_protocol_error.action,
                                       status_label, link_label, action_type);
      }
      return SYNC_ERROR;
    }

    // Since there is no auth in progress, check for an auth error first.
    if (auth_error.state() != GoogleServiceAuthError::NONE) {
      if (status_label && link_label) {
        GetStatusForAuthError(auth_error, status_label, link_label,
                              action_type);
      }
      return SYNC_ERROR;
    }

    // We don't have an auth error. Check for an actionable error.
    if (status_label && link_label) {
      GetStatusForActionableError(status.sync_protocol_error.action,
                                  status_label, link_label, action_type);
      if (!status_label->empty()) {
        return SYNC_ERROR;
      }
    }

    // Check for a passphrase error.
    if (service->GetUserSettings()->IsPassphraseRequiredForDecryption()) {
      if (status_label && link_label) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SYNC_STATUS_NEEDS_PASSWORD);
        *link_label = l10n_util::GetStringUTF16(
            IDS_SYNC_STATUS_NEEDS_PASSWORD_LINK_LABEL);
        *action_type = ENTER_PASSPHRASE;
      }
      return SYNC_ERROR;
    }

    if (status_label) {
      *status_label = GetSyncedStateStatusLabel(service);
    }

    // Check to see if sync has been disabled via the dasboard and needs to be
    // set up once again.
    if (service->HasDisableReason(
            syncer::SyncService::DISABLE_REASON_USER_CHOICE) &&
        status.sync_protocol_error.error_type == syncer::NOT_MY_BIRTHDAY) {
      return PRE_SYNCED;
    }

    // There is no error. Display "Last synced..." message.
    return SYNCED;
  }

  MessageType result_type = SYNCED;
  // Either show auth error information with a link to re-login, auth in prog,
  // or provide a link to continue with setup.
  if (service->IsFirstSetupInProgress()) {
    result_type = PRE_SYNCED;
    if (status_label) {
      *status_label = l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS);
    }

    if (auth_error.state() != GoogleServiceAuthError::NONE &&
        auth_error.state() != GoogleServiceAuthError::TWO_FACTOR) {
      if (status_label && link_label) {
        GetStatusForAuthError(auth_error, status_label, link_label,
                              action_type);
      }
      result_type = SYNC_ERROR;
    }
  } else if (service->HasUnrecoverableError()) {
    result_type = SYNC_ERROR;
    if (status_label && link_label) {
      GetStatusForUnrecoverableError(is_user_signout_allowed,
                                     status.sync_protocol_error.action,
                                     status_label, link_label, action_type);
    }
  } else {
    if (ShouldRequestSyncConfirmation(service)) {
      if (status_label && link_label) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SYNC_SETTINGS_NOT_CONFIRMED);
        *link_label = l10n_util::GetStringUTF16(
            IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON);
      }
      *action_type = CONFIRM_SYNC_SETTINGS;
      result_type = SYNC_ERROR;
    } else {
      // The user is signed in, but sync has been stopped.
      result_type = PRE_SYNCED;
      if (status_label) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_SUPPRESSED);
      }
    }
  }
  return result_type;
}

}  // namespace

MessageType GetStatusLabels(Profile* profile,
                            base::string16* status_label,
                            base::string16* link_label,
                            ActionType* action_type) {
  DCHECK(profile);
  syncer::SyncService* service =
      ProfileSyncServiceFactory::GetSyncServiceForProfile(profile);
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const bool is_user_signout_allowed =
      signin_util::IsUserSignoutAllowedForProfile(profile);
  GoogleServiceAuthError auth_error =
      SigninErrorControllerFactory::GetForProfile(profile)->auth_error();
  return GetStatusLabelsImpl(service, identity_manager, is_user_signout_allowed,
                             auth_error, status_label, link_label, action_type);
}

MessageType GetStatus(Profile* profile) {
  ActionType action_type = NO_ACTION;
  return GetStatusLabels(profile, /*status_label=*/nullptr,
                         /*link_label=*/nullptr, &action_type);
}

#if !defined(OS_CHROMEOS)
AvatarSyncErrorType GetMessagesForAvatarSyncError(
    Profile* profile,
    int* content_string_id,
    int* button_string_id) {
  const syncer::SyncService* service =
      ProfileSyncServiceFactory::GetSyncServiceForProfile(profile);

  // The order or priority is going to be: 1. Unrecoverable errors.
  // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.
  if (service && service->HasUnrecoverableError()) {
    // An unrecoverable error is sometimes accompanied by an actionable error.
    // If an actionable error is not set to be UPGRADE_CLIENT, then show a
    // generic unrecoverable error message.
    syncer::SyncStatus status;
    service->QueryDetailedSyncStatus(&status);
    if (status.sync_protocol_error.action != syncer::UPGRADE_CLIENT) {
      // Display different messages and buttons for managed accounts.
      if (!signin_util::IsUserSignoutAllowedForProfile(profile)) {
        // For a managed user, the user is directed to the signout
        // confirmation dialogue in the settings page.
        *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNOUT_MESSAGE;
        *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON;
        return MANAGED_USER_UNRECOVERABLE_ERROR;
      }
      // For a non-managed user, we sign out on the user's behalf and prompt
      // the user to sign in again.
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_BUTTON;
      return UNRECOVERABLE_ERROR;
    }
  }

  // Check for an auth error.
  SigninErrorController* signin_error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (signin_error_controller && signin_error_controller->HasError()) {
    // The user can reauth to resolve the signin error.
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_BUTTON;
    return AUTH_ERROR;
  }

  // Check for sync errors if the sync service is enabled.
  if (service) {
    // Check for an actionable UPGRADE_CLIENT error.
    syncer::SyncStatus status;
    service->QueryDetailedSyncStatus(&status);
    if (status.sync_protocol_error.action == syncer::UPGRADE_CLIENT) {
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_UPGRADE_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON;
      return UPGRADE_CLIENT_ERROR;
    }

    // Check for a sync passphrase error.
    if (ShouldShowPassphraseError(service)) {
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON;
      return PASSPHRASE_ERROR;
    }

    // Check for a sync confirmation error.
    if (ShouldRequestSyncConfirmation(service)) {
      *content_string_id = IDS_SYNC_SETTINGS_NOT_CONFIRMED;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON;
      return SETTINGS_UNCONFIRMED_ERROR;
    }
  }

  // There is no error.
  return NO_SYNC_ERROR;
}
#endif  // !defined(OS_CHROMEOS)

bool ShouldRequestSyncConfirmation(const syncer::SyncService* service) {
  return !service->IsLocalSyncEnabled() &&
         service->GetUserSettings()->IsSyncRequested() &&
         service->IsAuthenticatedAccountPrimary() &&
         !service->IsSetupInProgress() &&
         !service->GetUserSettings()->IsFirstSetupComplete();
}

bool ShouldShowPassphraseError(const syncer::SyncService* service) {
  return service->GetUserSettings()->IsFirstSetupComplete() &&
         service->GetUserSettings()->IsPassphraseRequiredForDecryption();
}

}  // namespace sync_ui_util
