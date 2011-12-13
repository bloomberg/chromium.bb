// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/proto_enum_conversions.h"
#include "chrome/browser/sync/protocol/sync_protocol_error.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

typedef GoogleServiceAuthError AuthError;

namespace sync_ui_util {

namespace {

// Given an authentication state this helper function returns various labels
// that can be used to display information about the state.
void GetStatusLabelsForAuthError(const AuthError& auth_error,
                                 const ProfileSyncService& service,
                                 string16* status_label,
                                 string16* link_label,
                                 string16* global_error_menu_label,
                                 string16* global_error_bubble_message,
                                 string16* global_error_bubble_accept_label) {
  string16 username = UTF8ToUTF16(service.profile()->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername));
  string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  if (link_label)
    link_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL));

  switch (auth_error.state()) {
    case AuthError::INVALID_GAIA_CREDENTIALS:
    case AuthError::ACCOUNT_DELETED:
    case AuthError::ACCOUNT_DISABLED:
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
        if (global_error_menu_label) {
          global_error_menu_label->assign(l10n_util::GetStringUTF16(
              IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM));
        }
        if (global_error_bubble_message) {
          global_error_bubble_message->assign(l10n_util::GetStringFUTF16(
              IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE, product_name));
        }
        if (global_error_bubble_accept_label) {
          global_error_bubble_accept_label->assign(l10n_util::GetStringUTF16(
              IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_ACCEPT));
        }
      }
      break;
    case AuthError::SERVICE_UNAVAILABLE:
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE));
      }
      if (link_label)
        link_label->clear();
      if (global_error_menu_label) {
        global_error_menu_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM));
      }
      if (global_error_bubble_message) {
        global_error_bubble_message->assign(l10n_util::GetStringFUTF16(
            IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_MESSAGE, product_name));
      }
      if (global_error_bubble_accept_label) {
        global_error_bubble_accept_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_ACCEPT));
      }
      break;
    case AuthError::CONNECTION_FAILED:
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
      if (global_error_menu_label) {
        global_error_menu_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM));
      }
      if (global_error_bubble_message) {
        global_error_bubble_message->assign(l10n_util::GetStringFUTF16(
            IDS_SYNC_OTHER_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE, product_name));
      }
      if (global_error_bubble_accept_label) {
        global_error_bubble_accept_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_ACCEPT));
      }
      break;
  }
}

// Returns the message that should be displayed when the user is authenticated
// and can connect to the sync server. If the user hasn't yet authenticated, an
// empty string is returned.
string16 GetSyncedStateStatusLabel(ProfileSyncService* service,
                                   StatusLabelStyle style) {
  if (!service->sync_initialized())
    return string16();

  string16 user_name = UTF8ToUTF16(service->profile()->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername));
  DCHECK(!user_name.empty());

  // Message may also carry additional advice with an HTML link, if acceptable.
  switch (style) {
    case PLAIN_TEXT:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER,
          user_name);
    case WITH_HTML:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER_WITH_MANAGE_LINK,
          user_name,
          ASCIIToUTF16(chrome::kSyncGoogleDashboardURL));
    default:
      NOTREACHED();
      return NULL;
  }
}

void GetStatusForActionableError(
    const browser_sync::SyncProtocolError& error,
    string16* status_label) {
  DCHECK(status_label);
  switch (error.action) {
    case browser_sync::STOP_AND_RESTART_SYNC:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_STOP_AND_RESTART_SYNC));
      break;
    case browser_sync::UPGRADE_CLIENT:
       status_label->assign(
           l10n_util::GetStringFUTF16(IDS_SYNC_UPGRADE_CLIENT,
               l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
      break;
    case browser_sync::ENABLE_SYNC_ON_ACCOUNT:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_ENABLE_SYNC_ON_ACCOUNT));
    break;
    case browser_sync::CLEAR_USER_DATA_AND_RESYNC:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_CLEAR_USER_DATA));
      break;
    default:
      NOTREACHED();
  }
}

// TODO(akalin): Write unit tests for these three functions below.

// status_label and link_label must either be both NULL or both non-NULL.
MessageType GetStatusInfo(ProfileSyncService* service,
                          StatusLabelStyle style,
                          string16* status_label,
                          string16* link_label) {
  DCHECK_EQ(status_label == NULL, link_label == NULL);

  MessageType result_type(SYNCED);

  if (!service) {
    return PRE_SYNCED;
  }

  if (service->HasSyncSetupCompleted()) {
    ProfileSyncService::Status status(service->QueryDetailedSyncStatus());
    const AuthError& auth_error = service->GetAuthError();

    // The order or priority is going to be: 1. Unrecoverable errors.
    // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.

    if (service->unrecoverable_error_detected()) {
      if (status_label) {
        status_label->assign(l10n_util::GetStringFUTF16(
            IDS_SYNC_STATUS_UNRECOVERABLE_ERROR,
            l10n_util::GetStringUTF16(IDS_SYNC_UNRECOVERABLE_ERROR_HELP_URL)));
      }
      return SYNC_ERROR;
    }

    // For auth errors first check if an auth is in progress.
    if (service->UIShouldDepictAuthInProgress()) {
      if (status_label) {
        status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
      }
      return PRE_SYNCED;
    }

    // No auth in progress check for an auth error.
    if (auth_error.state() != AuthError::NONE) {
      if (status_label && link_label) {
        GetStatusLabelsForAuthError(auth_error, *service,
                                    status_label, link_label, NULL, NULL, NULL);
      }
      return SYNC_ERROR;
    }

    // We dont have an auth error. Check for protocol error.
    if (ShouldShowActionOnUI(status.sync_protocol_error)) {
      if (status_label) {
        GetStatusForActionableError(status.sync_protocol_error,
            status_label);
      }
      return SYNC_ERROR;
    }

    // Now finally passphrase error.
    if (service->IsPassphraseRequired()) {
      if (service->IsPassphraseRequiredForDecryption()) {
        // TODO(lipalani) : Ask tim if this is still needed.
        // NOT first machine.
        // Show a link ("needs attention"), but still indicate the
        // current synced status.  Return SYNC_PROMO so that
        // the configure link will still be shown.
        if (status_label && link_label) {
          status_label->assign(GetSyncedStateStatusLabel(service, style));
          link_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_PASSWORD_SYNC_ATTENTION));
        }
        return SYNC_PROMO;
      }
    }

    // There is no error. Display "Last synced..." message.
    if (status_label)
      status_label->assign(GetSyncedStateStatusLabel(service, style));
    return SYNCED;
  } else {
    // Either show auth error information with a link to re-login, auth in prog,
    // or provide a link to continue with setup.
    result_type = PRE_SYNCED;
    if (service->SetupInProgress()) {
      ProfileSyncService::Status status(service->QueryDetailedSyncStatus());
      const AuthError& auth_error = service->GetAuthError();
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS));
      }
      if (service->UIShouldDepictAuthInProgress()) {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
        }
      } else if (auth_error.state() != AuthError::NONE &&
                 auth_error.state() != AuthError::TWO_FACTOR) {
        if (status_label) {
          status_label->clear();
          GetStatusLabelsForAuthError(auth_error, *service, status_label, NULL,
                                      NULL, NULL, NULL);
        }
        result_type = SYNC_ERROR;
      } else if (!status.authenticated) {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_ACCOUNT_DETAILS_NOT_ENTERED));
        }
      }
    } else if (service->unrecoverable_error_detected()) {
      result_type = SYNC_ERROR;
      ProfileSyncService::Status status(service->QueryDetailedSyncStatus());
      if (ShouldShowActionOnUI(status.sync_protocol_error)) {
        if (status_label) {
          GetStatusForActionableError(status.sync_protocol_error,
              status_label);
        }
      } else if (status_label) {
        status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_SETUP_ERROR));
      }
    }
  }
  return result_type;
}

// Returns the status info for use on the new tab page, where we want slightly
// different information than in the settings panel.
MessageType GetStatusInfoForNewTabPage(ProfileSyncService* service,
                                       string16* status_label,
                                       string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);

  if (service->HasSyncSetupCompleted() &&
      service->IsPassphraseRequired()) {
    if (service->passphrase_required_reason() == sync_api::REASON_ENCRYPTION) {
      // First machine migrating to passwords.  Show as a promotion.
      if (status_label && link_label) {
        status_label->assign(
            l10n_util::GetStringFUTF16(
                IDS_SYNC_NTP_PASSWORD_PROMO,
                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
        link_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_NTP_PASSWORD_ENABLE));
      }
      return SYNC_PROMO;
    } else {
      // NOT first machine.
      // Show a link and present as an error ("needs attention").
      if (status_label && link_label) {
        status_label->assign(string16());
        link_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_CONFIGURE_ENCRYPTION));
      }
      return SYNC_ERROR;
    }
  }

  // Fallback to default.
  return GetStatusInfo(service, WITH_HTML, status_label, link_label);
}

}  // namespace

MessageType GetStatusLabels(ProfileSyncService* service,
                            StatusLabelStyle style,
                            string16* status_label,
                            string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);
  return sync_ui_util::GetStatusInfo(service, style, status_label, link_label);
}

MessageType GetStatusLabelsForNewTabPage(ProfileSyncService* service,
                                         string16* status_label,
                                         string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);
  return sync_ui_util::GetStatusInfoForNewTabPage(
      service, status_label, link_label);
}

void GetStatusLabelsForSyncGlobalError(ProfileSyncService* service,
                                       string16* menu_label,
                                       string16* bubble_message,
                                       string16* bubble_accept_label) {
  DCHECK(menu_label);
  DCHECK(bubble_message);
  DCHECK(bubble_accept_label);
  *menu_label = string16();
  *bubble_message = string16();
  *bubble_accept_label = string16();

  if (!service->HasSyncSetupCompleted())
    return;

  if (service->IsPassphraseRequired() &&
      service->IsPassphraseRequiredForDecryption()) {
    // This is not the first machine so ask user to enter passphrase.
    *menu_label = l10n_util::GetStringUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_WRENCH_MENU_ITEM);
    string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
    *bubble_message = l10n_util::GetStringFUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_MESSAGE, product_name);
    *bubble_accept_label = l10n_util::GetStringUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_ACCEPT);
    return;
  }

  MessageType status = GetStatus(service);
  if (status != SYNC_ERROR)
    return;

  const AuthError& auth_error = service->GetAuthError();
  if (auth_error.state() != AuthError::NONE) {
    GetStatusLabelsForAuthError(auth_error, *service, NULL, NULL,
        menu_label, bubble_message, bubble_accept_label);
  }
}

MessageType GetStatus(ProfileSyncService* service) {
  return sync_ui_util::GetStatusInfo(service, WITH_HTML, NULL, NULL);
}

string16 GetSyncMenuLabel(ProfileSyncService* service) {
  MessageType type = GetStatus(service);

  if (type == sync_ui_util::SYNCED)
    return l10n_util::GetStringUTF16(IDS_SYNC_MENU_SYNCED_LABEL);
  else if (type == sync_ui_util::SYNC_ERROR)
    return l10n_util::GetStringUTF16(IDS_SYNC_MENU_SYNC_ERROR_LABEL);
  else
    return l10n_util::GetStringUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL);
}

void OpenSyncMyBookmarksDialog(Profile* profile,
                               Browser* browser,
                               ProfileSyncService::SyncEventCodes code) {
  ProfileSyncService* service =
    profile->GetOriginalProfile()->GetProfileSyncService();
  if (!service || !service->IsSyncEnabled()) {
    LOG(DFATAL) << "OpenSyncMyBookmarksDialog called with sync disabled";
    return;
  }

  if (service->HasSyncSetupCompleted()) {
    bool create_window = browser == NULL;
    if (create_window)
      browser = Browser::Create(profile);
    browser->ShowOptionsTab(chrome::kPersonalOptionsSubPage);
    if (create_window)
      browser->window()->Show();
  } else {
    service->ShowLoginDialog();
    ProfileSyncService::SyncEvent(code);  // UMA stats
  }
}

void AddBoolSyncDetail(ListValue* details,
                       const std::string& stat_name,
                       bool stat_value) {
  DictionaryValue* val = new DictionaryValue;
  val->SetString("stat_name", stat_name);
  val->SetBoolean("stat_value", stat_value);
  details->Append(val);
}

void AddStringSyncDetails(ListValue* details, const std::string& stat_name,
                          const std::string& stat_value) {
  DictionaryValue* val = new DictionaryValue;
  val->SetString("stat_name", stat_name);
  val->SetString("stat_value", stat_value);
  details->Append(val);
}

void AddIntSyncDetail(ListValue* details, const std::string& stat_name,
                      int64 stat_value) {
  DictionaryValue* val = new DictionaryValue;
  val->SetString("stat_name", stat_name);
  val->SetString("stat_value", base::FormatNumber(stat_value));
  details->Append(val);
}

string16 ConstructTime(int64 time_in_int) {
  base::Time time = base::Time::FromInternalValue(time_in_int);

  // If time is null the format function returns a time in 1969.
  if (time.is_null())
    return string16();
  return base::TimeFormatFriendlyDateAndTime(time);
}

std::string MakeSyncAuthErrorText(
    const GoogleServiceAuthError::State& state) {
  switch (state) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return "INVALID_GAIA_CREDENTIALS";
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      return "USER_NOT_SIGNED_UP";
    case GoogleServiceAuthError::CONNECTION_FAILED:
      return "CONNECTION_FAILED";
    default:
      return std::string();
  }
}

void ConstructAboutInformation(ProfileSyncService* service,
                               DictionaryValue* strings) {
  CHECK(strings);
  if (!service) {
    strings->SetString("summary", "SYNC DISABLED");
  } else {
    sync_api::SyncManager::Status full_status(
        service->QueryDetailedSyncStatus());

    strings->SetString("service_url", service->sync_service_url().spec());
    strings->SetString("summary",
                       ProfileSyncService::BuildSyncStatusSummaryText(
                       full_status.summary));

    strings->SetString("version", GetVersionString());
    strings->Set("authenticated",
                 new base::FundamentalValue(full_status.authenticated));
    strings->SetString("auth_problem",
                       sync_ui_util::MakeSyncAuthErrorText(
                       service->GetAuthError().state()));

    strings->SetString("time_since_sync", service->GetLastSyncedTimeString());

    ListValue* details = new ListValue();
    strings->Set("details", details);
    sync_ui_util::AddBoolSyncDetail(details, "Sync Initialized",
                                    service->sync_initialized());
    sync_ui_util::AddBoolSyncDetail(details, "Sync Setup Has Completed",
                                    service->HasSyncSetupCompleted());
    sync_ui_util::AddStringSyncDetails(
        details,
        "Client ID",
        full_status.unique_id.empty() ? "none" : full_status.unique_id);
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Server Up",
                                    full_status.server_up);
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Server Reachable",
                                    full_status.server_reachable);
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Server Broken",
                                    full_status.server_broken);
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Notifications Enabled",
                                    full_status.notifications_enabled);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Notifications Received",
                                   full_status.notifications_received);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Notifiable Commits",
                                   full_status.notifiable_commits);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Unsynced Count",
                                   full_status.unsynced_count);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Conflicting Count",
                                   full_status.conflicting_count);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Local Overwrites",
                                   full_status.num_local_overwrites_total);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Server Overwrites",
                                   full_status.num_server_overwrites_total);
    sync_ui_util::AddBoolSyncDetail(details, "Syncing", full_status.syncing);
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Initial Sync Ended",
                                    full_status.initial_sync_ended);
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Syncer Stuck",
                                    full_status.syncer_stuck);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Updates Available",
                                   full_status.updates_available);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Updates Downloaded (All)",
                                   full_status.updates_received);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Updates Downloaded (Tombstones)",
                                   full_status.tombstone_updates_received);
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Disk Full",
                                    full_status.disk_full);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Max Consecutive Errors",
                                   full_status.max_consecutive_errors);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Empty GetUpdates",
                                   full_status.empty_get_updates);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Nonempty GetUpdates",
                                   full_status.nonempty_get_updates);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Useless Sync Cycles",
                                   full_status.useless_sync_cycles);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Useful Sync Cycles",
                                   full_status.useful_sync_cycles);
    // Only safe to call IsUsingSecondaryPassphrase() if the backend is
    // initialized already - otherwise, we have no idea whether we are
    // using a secondary passphrase or not.
    if (service->sync_initialized()) {
      sync_ui_util::AddBoolSyncDetail(details,
                                      "Explicit Passphrase",
                                      service->IsUsingSecondaryPassphrase());
    }
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Passphrase Required",
                                    service->IsPassphraseRequired());
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Cryptographer Ready",
                                    full_status.cryptographer_ready);
    sync_ui_util::AddBoolSyncDetail(details,
                                    "Cryptographer Has Pending Keys",
                                    full_status.crypto_has_pending_keys);
    sync_ui_util::AddStringSyncDetails(details,
        "Encrypted Types",
        syncable::ModelTypeSetToString(full_status.encrypted_types));

    const browser_sync::sessions::SyncSessionSnapshot* snapshot =
        service->sync_initialized() ?
        service->GetLastSessionSnapshot() : NULL;

    // |snapshot| could be NULL if sync is not yet initialized.
    if (snapshot) {
      sync_ui_util::AddIntSyncDetail(details, "Download Count (This Session)",
          snapshot->syncer_status.num_updates_downloaded_total);
      sync_ui_util::AddIntSyncDetail(details, "Commit Count (This Session)",
          snapshot->syncer_status.num_successful_commits);
      sync_ui_util::AddStringSyncDetails(details, "Last Sync Source",
          browser_sync::GetUpdatesSourceString(
          snapshot->source.updates_source));

      // Print the count of entries from snapshot. Warning: This might be
      // slightly out of date if there are client side changes that are yet
      // unsynced  However if we query the latest count here we will
      // have to hold the transaction which means we cannot display this page
      // when syncing.
      sync_ui_util::AddIntSyncDetail(details, "Entries" ,
                                     snapshot->num_entries);
      sync_ui_util::AddBoolSyncDetail(details, "Throttled",
                                      snapshot->is_silenced);
    }

    // Now set the actionable errors.
    ListValue* actionable_error = new ListValue();
    strings->Set("actionable_error", actionable_error);
    sync_ui_util::AddStringSyncDetails(actionable_error, "Error Type",
        browser_sync::GetSyncErrorTypeString(
            full_status.sync_protocol_error.error_type));
    sync_ui_util::AddStringSyncDetails(actionable_error, "Action",
        browser_sync::GetClientActionString(
            full_status.sync_protocol_error.action));
    sync_ui_util::AddStringSyncDetails(actionable_error, "url",
        full_status.sync_protocol_error.url);
    sync_ui_util::AddStringSyncDetails(actionable_error, "Error Description",
        full_status.sync_protocol_error.error_description);

    const FailedDatatypesHandler& failed_datatypes_handler =
        service->failed_datatypes_handler();
    if (failed_datatypes_handler.AnyFailedDatatype()) {
      strings->SetString("failed_data_types",
          failed_datatypes_handler.GetErrorString());
    }

    if (service->unrecoverable_error_detected()) {
      strings->Set("unrecoverable_error_detected",
                   new base::FundamentalValue(true));
      tracked_objects::Location loc(service->unrecoverable_error_location());
      std::string location_str;
      loc.Write(true, true, &location_str);
      std::string unrecoverable_error_message =
          "Unrecoverable error detected at " + location_str +
          ": " + service->unrecoverable_error_message();
      strings->SetString("unrecoverable_error_message",
                         unrecoverable_error_message);
    } else if (service->sync_initialized()) {
      browser_sync::ModelSafeRoutingInfo routes;
      service->GetModelSafeRoutingInfo(&routes);
      ListValue* routing_info = new ListValue();
      strings->Set("routing_info", routing_info);
      browser_sync::ModelSafeRoutingInfo::const_iterator it = routes.begin();
      for (; it != routes.end(); ++it) {
        DictionaryValue* val = new DictionaryValue;
        val->SetString("model_type", ModelTypeToString(it->first));
        val->SetString("group", ModelSafeGroupToString(it->second));
        routing_info->Append(val);
      }
    }
  }
}

std::string GetVersionString() {
  // Build a version string that matches MakeUserAgentForSyncApi with the
  // addition of channel info and proper OS names.
  chrome::VersionInfo chrome_version;
  if (!chrome_version.is_valid())
    return "invalid";
  // GetVersionStringModifier returns empty string for stable channel or
  // unofficial builds, the channel string otherwise. We want to have "-devel"
  // for unofficial builds only.
  std::string version_modifier =
      chrome::VersionInfo::GetVersionStringModifier();
  if (version_modifier.empty()) {
    if (chrome::VersionInfo::GetChannel() !=
            chrome::VersionInfo::CHANNEL_STABLE) {
      version_modifier = "-devel";
    }
  } else {
    version_modifier = " " + version_modifier;
  }
  return chrome_version.Name() + " " + chrome_version.OSType() + " " +
      chrome_version.Version() + " (" + chrome_version.LastChange() + ")" +
      version_modifier;
}

}  // namespace sync_ui_util
