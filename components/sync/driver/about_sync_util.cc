// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/about_sync_util.h"

#include <string>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/model/time.h"
#include "components/sync/protocol/proto_enum_conversions.h"

using base::DictionaryValue;
using base::ListValue;

namespace syncer {

namespace sync_ui_util {

const char kIdentityTitle[] = "Identity";
const char kDetailsKey[] = "details";

// Resource paths.
const char kAboutJS[] = "about.js";
const char kChromeSyncJS[] = "chrome_sync.js";
const char kDataJS[] = "data.js";
const char kEventsJS[] = "events.js";
const char kSearchJS[] = "search.js";
const char kSyncIndexJS[] = "sync_index.js";
const char kSyncLogJS[] = "sync_log.js";
const char kSyncNodeBrowserJS[] = "sync_node_browser.js";
const char kSyncSearchJS[] = "sync_search.js";
const char kTypesJS[] = "types.js";

// Message handlers.
const char kDispatchEvent[] = "chrome.sync.dispatchEvent";
const char kGetAllNodes[] = "getAllNodes";
const char kGetAllNodesCallback[] = "chrome.sync.getAllNodesCallback";
const char kRegisterForEvents[] = "registerForEvents";
const char kRegisterForPerTypeCounters[] = "registerForPerTypeCounters";
const char kRequestListOfTypes[] = "requestListOfTypes";
const char kRequestUpdatedAboutInfo[] = "requestUpdatedAboutInfo";

// Other strings.
const char kCommit[] = "commit";
const char kCounters[] = "counters";
const char kCounterType[] = "counterType";
const char kModelType[] = "modelType";
const char kOnAboutInfoUpdated[] = "onAboutInfoUpdated";
const char kOnCountersUpdated[] = "onCountersUpdated";
const char kOnProtocolEvent[] = "onProtocolEvent";
const char kOnReceivedListOfTypes[] = "onReceivedListOfTypes";
const char kStatus[] = "status";
const char kTypes[] = "types";
const char kUpdate[] = "update";

namespace {

// Creates a 'section' for display on about:sync, consisting of a title and a
// list of fields.  Returns a pointer to the new section.  Note that
// |parent_list|, not the caller, owns the newly added section.
base::ListValue* AddSection(base::ListValue* parent_list,
                            const std::string& title) {
  std::unique_ptr<base::DictionaryValue> section(new base::DictionaryValue());
  base::ListValue* section_contents = new base::ListValue();
  section->SetString("title", title);
  section->Set("data", section_contents);
  section->SetBoolean("is_sensitive", false);
  parent_list->Append(std::move(section));
  return section_contents;
}

// Same as AddSection, but for data that should be elided when dumped into text
// form and posted in a public forum (e.g. unique identifiers).
base::ListValue* AddSensitiveSection(base::ListValue* parent_list,
                                     const std::string& title) {
  std::unique_ptr<base::DictionaryValue> section(new base::DictionaryValue());
  base::ListValue* section_contents = new base::ListValue();
  section->SetString("title", title);
  section->Set("data", section_contents);
  section->SetBoolean("is_sensitive", true);
  parent_list->Append(std::move(section));
  return section_contents;
}

// The following helper classes help manage the about:sync fields which will be
// populated in method in ConstructAboutInformation.
//
// Each instance of one of thse classes indicates a field in about:sync.  Each
// field will be serialized to a DictionaryValue with entries for 'stat_name',
// 'stat_value' and 'is_valid'.

class StringSyncStat {
 public:
  StringSyncStat(base::ListValue* section, const std::string& key);
  void SetValue(const std::string& value);
  void SetValue(const base::string16& value);

 private:
  // Owned by the |section| passed in during construction.
  base::DictionaryValue* stat_;
};

StringSyncStat::StringSyncStat(base::ListValue* section,
                               const std::string& key) {
  stat_ = new base::DictionaryValue();
  stat_->SetString("stat_name", key);
  stat_->SetString("stat_value", "Uninitialized");
  stat_->SetBoolean("is_valid", false);
  section->Append(base::WrapUnique(stat_));
}

void StringSyncStat::SetValue(const std::string& value) {
  stat_->SetString("stat_value", value);
  stat_->SetBoolean("is_valid", true);
}

void StringSyncStat::SetValue(const base::string16& value) {
  stat_->SetString("stat_value", value);
  stat_->SetBoolean("is_valid", true);
}

class BoolSyncStat {
 public:
  BoolSyncStat(base::ListValue* section, const std::string& key);
  void SetValue(bool value);

 private:
  // Owned by the |section| passed in during construction.
  base::DictionaryValue* stat_;
};

BoolSyncStat::BoolSyncStat(base::ListValue* section, const std::string& key) {
  stat_ = new base::DictionaryValue();
  stat_->SetString("stat_name", key);
  stat_->SetBoolean("stat_value", false);
  stat_->SetBoolean("is_valid", false);
  section->Append(base::WrapUnique(stat_));
}

void BoolSyncStat::SetValue(bool value) {
  stat_->SetBoolean("stat_value", value);
  stat_->SetBoolean("is_valid", true);
}

class IntSyncStat {
 public:
  IntSyncStat(base::ListValue* section, const std::string& key);
  void SetValue(int value);

 private:
  // Owned by the |section| passed in during construction.
  base::DictionaryValue* stat_;
};

IntSyncStat::IntSyncStat(base::ListValue* section, const std::string& key) {
  stat_ = new base::DictionaryValue();
  stat_->SetString("stat_name", key);
  stat_->SetInteger("stat_value", 0);
  stat_->SetBoolean("is_valid", false);
  section->Append(base::WrapUnique(stat_));
}

void IntSyncStat::SetValue(int value) {
  stat_->SetInteger("stat_value", value);
  stat_->SetBoolean("is_valid", true);
}

// Returns a string describing the chrome version environment. Version format:
// <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
// If version information is unavailable, returns "invalid."
// TODO(zea): this approximately matches MakeUserAgentForSyncApi in
// sync_backend_host.cc. Unify the two if possible.
std::string GetVersionString(version_info::Channel channel) {
  // Build a version string that matches MakeUserAgentForSyncApi with the
  // addition of channel info and proper OS names.
  // chrome::GetChannelString() returns empty string for stable channel or
  // unofficial builds, the channel string otherwise. We want to have "-devel"
  // for unofficial builds only.
  std::string version_modifier = version_info::GetChannelString(channel);
  if (version_modifier.empty()) {
    if (channel != version_info::Channel::STABLE) {
      version_modifier = "-devel";
    }
  } else {
    version_modifier = " " + version_modifier;
  }
  return version_info::GetProductName() + " " + version_info::GetOSType() +
         " " + version_info::GetVersionNumber() + " (" +
         version_info::GetLastChange() + ")" + version_modifier;
}

std::string GetTimeStr(base::Time time, const std::string& default_msg) {
  std::string time_str;
  if (time.is_null())
    time_str = default_msg;
  else
    time_str = GetTimeDebugString(time);
  return time_str;
}

std::string GetConnectionStatus(const SyncService::SyncTokenStatus& status) {
  std::string message;
  switch (status.connection_status) {
    case CONNECTION_NOT_ATTEMPTED:
      base::StringAppendF(&message, "not attempted");
      break;
    case CONNECTION_OK:
      base::StringAppendF(
          &message, "OK since %s",
          GetTimeStr(status.connection_status_update_time, "n/a").c_str());
      break;
    case CONNECTION_AUTH_ERROR:
      base::StringAppendF(
          &message, "auth error since %s",
          GetTimeStr(status.connection_status_update_time, "n/a").c_str());
      break;
    case CONNECTION_SERVER_ERROR:
      base::StringAppendF(
          &message, "server error since %s",
          GetTimeStr(status.connection_status_update_time, "n/a").c_str());
      break;
    default:
      NOTREACHED();
  }
  return message;
}

}  // namespace

// This function both defines the structure of the message to be returned and
// its contents.  Most of the message consists of simple fields in about:sync
// which are grouped into sections and populated with the help of the SyncStat
// classes defined above.
std::unique_ptr<base::DictionaryValue> ConstructAboutInformation(
    SyncService* service,
    SigninManagerBase* signin,
    version_info::Channel channel) {
  std::unique_ptr<base::DictionaryValue> about_info(
      new base::DictionaryValue());

  // 'details': A list of sections.
  base::ListValue* stats_list = new base::ListValue();

  // The following lines define the sections and their fields.  For each field,
  // a class is instantiated, which allows us to reference the fields in
  // 'setter' code later on in this function.
  base::ListValue* section_summary = AddSection(stats_list, "Summary");
  StringSyncStat summary_string(section_summary, "Summary");

  base::ListValue* section_version = AddSection(stats_list, "Version Info");
  StringSyncStat client_version(section_version, "Client Version");
  StringSyncStat server_url(section_version, "Server URL");

  base::ListValue* section_identity =
      AddSensitiveSection(stats_list, kIdentityTitle);
  StringSyncStat sync_id(section_identity, "Sync Client ID");
  StringSyncStat invalidator_id(section_identity, "Invalidator Client ID");
  StringSyncStat username(section_identity, "Username");

  base::ListValue* section_credentials = AddSection(stats_list, "Credentials");
  StringSyncStat request_token_time(section_credentials, "Requested Token");
  StringSyncStat receive_token_time(section_credentials, "Received Token");
  StringSyncStat token_request_status(section_credentials,
                                      "Token Request Status");
  StringSyncStat next_token_request(section_credentials, "Next Token Request");

  base::ListValue* section_local = AddSection(stats_list, "Local State");
  StringSyncStat server_connection(section_local, "Server Connection");
  StringSyncStat last_synced(section_local, "Last Synced");
  BoolSyncStat is_setup_complete(section_local,
                                 "Sync First-Time Setup Complete");
  StringSyncStat backend_initialization(section_local,
                                        "Sync Backend Initialization");
  BoolSyncStat is_syncing(section_local, "Syncing");
  BoolSyncStat is_local_sync_enabled(section_local,
                                     "Local sync backend enabled");
  StringSyncStat local_backend_path(section_local, "Local backend path");

  base::ListValue* section_network = AddSection(stats_list, "Network");
  BoolSyncStat is_throttled(section_network, "Throttled");
  StringSyncStat retry_time(section_network, "Retry time (maybe stale)");
  BoolSyncStat are_notifications_enabled(section_network,
                                         "Notifications Enabled");

  base::ListValue* section_encryption = AddSection(stats_list, "Encryption");
  BoolSyncStat is_using_explicit_passphrase(section_encryption,
                                            "Explicit Passphrase");
  BoolSyncStat is_passphrase_required(section_encryption,
                                      "Passphrase Required");
  BoolSyncStat is_cryptographer_ready(section_encryption,
                                      "Cryptographer Ready");
  BoolSyncStat has_pending_keys(section_encryption,
                                "Cryptographer Has Pending Keys");
  StringSyncStat encrypted_types(section_encryption, "Encrypted Types");
  BoolSyncStat has_keystore_key(section_encryption, "Has Keystore Key");
  StringSyncStat keystore_migration_time(section_encryption,
                                         "Keystore Migration Time");
  StringSyncStat passphrase_type(section_encryption, "Passphrase Type");
  StringSyncStat passphrase_time(section_encryption, "Passphrase Time");

  base::ListValue* section_last_session =
      AddSection(stats_list, "Status from Last Completed Session");
  StringSyncStat session_source(section_last_session, "Sync Source");
  StringSyncStat get_key_result(section_last_session, "GetKey Step Result");
  StringSyncStat download_result(section_last_session, "Download Step Result");
  StringSyncStat commit_result(section_last_session, "Commit Step Result");

  base::ListValue* section_counters = AddSection(stats_list, "Running Totals");
  IntSyncStat notifications_received(section_counters,
                                     "Notifications Received");
  IntSyncStat updates_received(section_counters, "Updates Downloaded");
  IntSyncStat tombstone_updates(section_counters, "Tombstone Updates");
  IntSyncStat reflected_updates(section_counters, "Reflected Updates");
  IntSyncStat successful_commits(section_counters, "Successful Commits");
  IntSyncStat conflicts_resolved_local_wins(section_counters,
                                            "Conflicts Resolved: Client Wins");
  IntSyncStat conflicts_resolved_server_wins(section_counters,
                                             "Conflicts Resolved: Server Wins");

  base::ListValue* section_this_cycle =
      AddSection(stats_list, "Transient Counters (this cycle)");
  IntSyncStat encryption_conflicts(section_this_cycle, "Encryption Conflicts");
  IntSyncStat hierarchy_conflicts(section_this_cycle, "Hierarchy Conflicts");
  IntSyncStat server_conflicts(section_this_cycle, "Server Conflicts");
  IntSyncStat committed_items(section_this_cycle, "Committed Items");

  base::ListValue* section_that_cycle = AddSection(
      stats_list, "Transient Counters (last cycle of last completed session)");
  IntSyncStat updates_downloaded(section_that_cycle, "Updates Downloaded");
  IntSyncStat committed_count(section_that_cycle, "Committed Count");
  IntSyncStat entries(section_that_cycle, "Entries");

  base::ListValue* section_nudge_info =
      AddSection(stats_list, "Nudge Source Counters");
  IntSyncStat nudge_source_notification(section_nudge_info,
                                        "Server Invalidations");
  IntSyncStat nudge_source_local(section_nudge_info, "Local Changes");
  IntSyncStat nudge_source_local_refresh(section_nudge_info, "Local Refreshes");

  // This list of sections belongs in the 'details' field of the returned
  // message.
  about_info->Set(kDetailsKey, stats_list);

  // Populate all the fields we declared above.
  client_version.SetValue(GetVersionString(channel));

  if (!service) {
    summary_string.SetValue("Sync service does not exist");
    return about_info;
  }

  SyncStatus full_status;
  bool is_status_valid = service->QueryDetailedSyncStatus(&full_status);
  bool sync_active = service->IsSyncActive();
  const SyncCycleSnapshot& snapshot = service->GetLastCycleSnapshot();

  if (is_status_valid)
    summary_string.SetValue(service->QuerySyncStatusSummaryString());

  server_url.SetValue(service->sync_service_url().spec());

  if (is_status_valid && !full_status.sync_id.empty())
    sync_id.SetValue(full_status.sync_id);
  if (is_status_valid && !full_status.invalidator_client_id.empty())
    invalidator_id.SetValue(full_status.invalidator_client_id);
  if (signin)
    username.SetValue(signin->GetAuthenticatedAccountInfo().email);

  const SyncService::SyncTokenStatus& token_status =
      service->GetSyncTokenStatus();
  server_connection.SetValue(GetConnectionStatus(token_status));
  request_token_time.SetValue(
      GetTimeStr(token_status.token_request_time, "n/a"));
  receive_token_time.SetValue(
      GetTimeStr(token_status.token_receive_time, "n/a"));
  std::string err = token_status.last_get_token_error.error_message();
  token_request_status.SetValue(err.empty() ? "OK" : err);
  next_token_request.SetValue(
      GetTimeStr(token_status.next_token_request_time, "not scheduled"));

  last_synced.SetValue(service->GetLastSyncedTimeString());
  is_setup_complete.SetValue(service->IsFirstSetupComplete());
  is_local_sync_enabled.SetValue(service->IsLocalSyncEnabled());
  if (service->IsLocalSyncEnabled() && is_status_valid) {
    local_backend_path.SetValue(full_status.local_sync_folder);
  }
  backend_initialization.SetValue(
      service->GetEngineInitializationStateString());
  if (is_status_valid) {
    is_syncing.SetValue(full_status.syncing);
    retry_time.SetValue(GetTimeStr(full_status.retry_time,
                                   "Scheduler is not in backoff or throttled"));
  }

  if (snapshot.is_initialized())
    is_throttled.SetValue(snapshot.is_silenced());
  if (is_status_valid) {
    are_notifications_enabled.SetValue(full_status.notifications_enabled);
  }

  if (sync_active) {
    is_using_explicit_passphrase.SetValue(
        service->IsUsingSecondaryPassphrase());
    is_passphrase_required.SetValue(service->IsPassphraseRequired());
    passphrase_time.SetValue(
        GetTimeStr(service->GetExplicitPassphraseTime(), "No Passphrase Time"));
  }
  if (is_status_valid) {
    is_cryptographer_ready.SetValue(full_status.cryptographer_ready);
    has_pending_keys.SetValue(full_status.crypto_has_pending_keys);
    encrypted_types.SetValue(ModelTypeSetToString(full_status.encrypted_types));
    has_keystore_key.SetValue(full_status.has_keystore_key);
    keystore_migration_time.SetValue(
        GetTimeStr(full_status.keystore_migration_time, "Not Migrated"));
    passphrase_type.SetValue(
        PassphraseTypeToString(full_status.passphrase_type));
  }

  if (snapshot.is_initialized()) {
    if (snapshot.legacy_updates_source() !=
        sync_pb::GetUpdatesCallerInfo::UNKNOWN) {
      session_source.SetValue(
          ProtoEnumToString(snapshot.legacy_updates_source()));
    }
    get_key_result.SetValue(GetSyncerErrorString(
        snapshot.model_neutral_state().last_get_key_result));
    download_result.SetValue(GetSyncerErrorString(
        snapshot.model_neutral_state().last_download_updates_result));
    commit_result.SetValue(
        GetSyncerErrorString(snapshot.model_neutral_state().commit_result));
  }

  if (is_status_valid) {
    notifications_received.SetValue(full_status.notifications_received);
    updates_received.SetValue(full_status.updates_received);
    tombstone_updates.SetValue(full_status.tombstone_updates_received);
    reflected_updates.SetValue(full_status.reflected_updates_received);
    successful_commits.SetValue(full_status.num_commits_total);
    conflicts_resolved_local_wins.SetValue(
        full_status.num_local_overwrites_total);
    conflicts_resolved_server_wins.SetValue(
        full_status.num_server_overwrites_total);
  }

  if (is_status_valid) {
    encryption_conflicts.SetValue(full_status.encryption_conflicts);
    hierarchy_conflicts.SetValue(full_status.hierarchy_conflicts);
    server_conflicts.SetValue(full_status.server_conflicts);
    committed_items.SetValue(full_status.committed_count);
  }

  if (is_status_valid) {
    nudge_source_notification.SetValue(full_status.nudge_source_notification);
    nudge_source_local.SetValue(full_status.nudge_source_local);
    nudge_source_local_refresh.SetValue(full_status.nudge_source_local_refresh);
  }

  if (snapshot.is_initialized()) {
    updates_downloaded.SetValue(
        snapshot.model_neutral_state().num_updates_downloaded_total);
    committed_count.SetValue(
        snapshot.model_neutral_state().num_successful_commits);
    entries.SetValue(snapshot.num_entries());
  }

  // The values set from this point onwards do not belong in the
  // details list.

  // We don't need to check is_status_valid here.
  // full_status.sync_protocol_error is exported directly from the
  // ProfileSyncService, even if the backend doesn't exist.
  const bool actionable_error_detected =
      full_status.sync_protocol_error.error_type != UNKNOWN_ERROR &&
      full_status.sync_protocol_error.error_type != SYNC_SUCCESS;

  about_info->SetBoolean("actionable_error_detected",
                         actionable_error_detected);

  // NOTE: We won't bother showing any of the following values unless
  // actionable_error_detected is set.

  base::ListValue* actionable_error = new base::ListValue();
  about_info->Set("actionable_error", actionable_error);

  StringSyncStat error_type(actionable_error, "Error Type");
  StringSyncStat action(actionable_error, "Action");
  StringSyncStat url(actionable_error, "URL");
  StringSyncStat description(actionable_error, "Error Description");

  if (actionable_error_detected) {
    error_type.SetValue(
        GetSyncErrorTypeString(full_status.sync_protocol_error.error_type));
    action.SetValue(
        GetClientActionString(full_status.sync_protocol_error.action));
    url.SetValue(full_status.sync_protocol_error.url);
    description.SetValue(full_status.sync_protocol_error.error_description);
  }

  about_info->SetBoolean("unrecoverable_error_detected",
                         service->HasUnrecoverableError());

  if (service->HasUnrecoverableError()) {
    tracked_objects::Location loc(service->unrecoverable_error_location());
    std::string location_str;
    loc.Write(true, true, &location_str);
    std::string unrecoverable_error_message =
        "Unrecoverable error detected at " + location_str + ": " +
        service->unrecoverable_error_message();
    about_info->SetString("unrecoverable_error_message",
                          unrecoverable_error_message);
  }

  about_info->Set("type_status", service->GetTypeStatusMap());

  return about_info;
}

}  // namespace sync_ui_util

}  // namespace syncer
