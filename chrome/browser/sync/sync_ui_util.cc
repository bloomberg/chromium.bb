// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

typedef GoogleServiceAuthError AuthError;

namespace sync_ui_util {

namespace {

// Given an authentication state, this helper function returns the appropriate
// status message and, if necessary, the text that should appear in the
// re-login link.
void GetStatusLabelsForAuthError(const AuthError& auth_error,
    ProfileSyncService* service, string16* status_label,
    string16* link_label) {
  if (link_label)
    link_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL));
  if (auth_error.state() == AuthError::INVALID_GAIA_CREDENTIALS ||
      auth_error.state() == AuthError::ACCOUNT_DELETED ||
      auth_error.state() == AuthError::ACCOUNT_DISABLED) {
    // If the user name is empty then the first login failed, otherwise the
    // credentials are out-of-date.
    if (service->GetAuthenticatedUsername().empty())
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_INVALID_USER_CREDENTIALS));
    else
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_INFO_OUT_OF_DATE));
  } else if (auth_error.state() == AuthError::SERVICE_UNAVAILABLE) {
    DCHECK(service->GetAuthenticatedUsername().empty());
    status_label->assign(
        l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE));
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
string16 GetSyncedStateStatusLabel(ProfileSyncService* service) {
  string16 label;
  string16 user_name(service->GetAuthenticatedUsername());
  if (user_name.empty())
    return label;

  return l10n_util::GetStringFUTF16(IDS_SYNC_ACCOUNT_SYNCED_TO_USER_WITH_TIME,
                                    user_name,
                                    service->GetLastSyncedTimeString());
}

// TODO(akalin): Write unit tests for these three functions below.

// status_label and link_label must either be both NULL or both non-NULL.
MessageType GetStatusInfo(ProfileSyncService* service,
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

    // Either show auth error information with a link to re-login, auth in prog,
    // or note that everything is OK with the last synced time.
    if (status.authenticated && !service->observed_passphrase_required()) {
      // Everything is peachy.
      if (status_label) {
        status_label->assign(GetSyncedStateStatusLabel(service));
      }
      DCHECK_EQ(auth_error.state(), AuthError::NONE);
    } else if (service->UIShouldDepictAuthInProgress()) {
      if (status_label) {
        status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
      }
      result_type = PRE_SYNCED;
    } else if (service->observed_passphrase_required()) {
      if (service->passphrase_required_for_decryption()) {
        // NOT first machine.
        // Show a link ("needs attention"), but still indicate the
        // current synced status.  Return SYNC_PROMO so that
        // the configure link will still be shown.
        if (status_label && link_label) {
          status_label->assign(GetSyncedStateStatusLabel(service));
          link_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_PASSWORD_SYNC_ATTENTION));
        }
        result_type = SYNC_PROMO;
      } else {
        // First machine.  Don't show promotion, just show everything
        // normal.
        if (status_label)
          status_label->assign(GetSyncedStateStatusLabel(service));
      }
    } else if (auth_error.state() != AuthError::NONE) {
      if (status_label && link_label) {
        GetStatusLabelsForAuthError(auth_error, service,
                                    status_label, link_label);
      }
      result_type = SYNC_ERROR;
    }
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
      } else if (auth_error.state() != AuthError::NONE) {
        if (status_label) {
          status_label->clear();
          GetStatusLabelsForAuthError(auth_error, service, status_label, NULL);
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
      if (status_label) {
        status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_SETUP_ERROR));
      }
    } else {
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringFUTF16(IDS_SYNC_NOT_SET_UP_INFO,
                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
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
      service->observed_passphrase_required()) {
    if (!service->passphrase_required_for_decryption()) {
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
  return GetStatusInfo(service, status_label, link_label);
}

}  // namespace

// Returns an HTML chunk for a login prompt related to encryption.
string16 GetLoginMessageForEncryption() {
  std::vector<std::string> subst;
  const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SYNC_ENCRYPTION_LOGIN_HTML));
  subst.push_back(l10n_util::GetStringUTF8(IDS_SYNC_PLEASE_SIGN_IN));
  subst.push_back(
      l10n_util::GetStringFUTF8(IDS_SYNC_LOGIN_FOR_ENCRYPTION,
                                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));

  return UTF8ToUTF16(ReplaceStringPlaceholders(html, subst, NULL));
}

MessageType GetStatusLabels(ProfileSyncService* service,
                            string16* status_label,
                            string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);
  return sync_ui_util::GetStatusInfo(service, status_label, link_label);
}

MessageType GetStatusLabelsForNewTabPage(ProfileSyncService* service,
                                         string16* status_label,
                                         string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);
  return sync_ui_util::GetStatusInfoForNewTabPage(
      service, status_label, link_label);
}

MessageType GetStatus(ProfileSyncService* service) {
  return sync_ui_util::GetStatusInfo(service, NULL, NULL);
}

bool ShouldShowSyncErrorButton(ProfileSyncService* service) {
  return service && !service->IsManaged() && service->HasSyncSetupCompleted() &&
      (GetStatus(service) == sync_ui_util::SYNC_ERROR);
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
    service->ShowLoginDialog(NULL);
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
  if (!service || !service->HasSyncSetupCompleted()) {
    strings->SetString("summary", "SYNC DISABLED");
  } else {
    sync_api::SyncManager::Status full_status(
        service->QueryDetailedSyncStatus());

    strings->SetString("service_url", service->sync_service_url().spec());
    strings->SetString("summary",
                       ProfileSyncService::BuildSyncStatusSummaryText(
                       full_status.summary));

    strings->Set("authenticated",
                 new FundamentalValue(full_status.authenticated));
    strings->SetString("auth_problem",
                       sync_ui_util::MakeSyncAuthErrorText(
                       service->GetAuthError().state()));

    strings->SetString("time_since_sync", service->GetLastSyncedTimeString());

    ListValue* details = new ListValue();
    strings->Set("details", details);
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
                                   "Notifications Sent",
                                   full_status.notifications_sent);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Unsynced Count",
                                   full_status.unsynced_count);
    sync_ui_util::AddIntSyncDetail(details,
                                   "Conflicting Count",
                                   full_status.conflicting_count);
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

    if (service->unrecoverable_error_detected()) {
      strings->Set("unrecoverable_error_detected", new FundamentalValue(true));
      strings->SetString("unrecoverable_error_message",
                         service->unrecoverable_error_message());
      tracked_objects::Location loc(service->unrecoverable_error_location());
      std::string location_str;
      loc.Write(true, true, &location_str);
      strings->SetString("unrecoverable_error_location", location_str);
    } else if (!service->sync_initialized()) {
      strings->SetString("summary", "Sync not yet initialized");
    } else {
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

      sync_ui_util::AddBoolSyncDetail(details,
          "Autofill Migrated",
          service->GetAutofillMigrationState() ==
          syncable::MIGRATED);
      syncable::AutofillMigrationDebugInfo info =
          service->GetAutofillMigrationDebugInfo();

      sync_ui_util::AddIntSyncDetail(details,
                                     "Bookmarks created during migration",
                                     info.bookmarks_added_during_migration);
      sync_ui_util::AddIntSyncDetail(details,
          "Autofill entries created during migration",
          info.autofill_entries_added_during_migration);
      sync_ui_util::AddIntSyncDetail(details,
          "Autofill Profiles created during migration",
          info.autofill_profile_added_during_migration);

      DictionaryValue* val = new DictionaryValue;
      val->SetString("stat_name", "Autofill Migration Time");
      val->SetString("stat_value", ConstructTime(info.autofill_migration_time));
      details->Append(val);
    }
  }
}

}  // namespace sync_ui_util
