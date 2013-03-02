// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/about_sync_util.h"

#include <string>

#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_version_info.h"
#include "sync/api/time.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"
#include "sync/protocol/proto_enum_conversions.h"

using base::DictionaryValue;
using base::ListValue;

const char kCredentialsTitle[] = "Credentials";
const char kDetailsKey[] = "details";

namespace {

// Creates a 'section' for display on about:sync, consisting of a title and a
// list of fields.  Returns a pointer to the new section.  Note that
// |parent_list|, not the caller, owns the newly added section.
ListValue* AddSection(ListValue* parent_list, const std::string& title) {
  DictionaryValue* section = new DictionaryValue();
  ListValue* section_contents = new ListValue();
  section->SetString("title", title);
  section->Set("data", section_contents);
  parent_list->Append(section);
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
  StringSyncStat(ListValue* section, const std::string& key);
  void SetValue(const std::string& value);
  void SetValue(const string16& value);

 private:
  // Owned by the |section| passed in during construction.
  DictionaryValue* stat_;
};

StringSyncStat::StringSyncStat(ListValue* section, const std::string& key) {
  stat_ = new DictionaryValue();
  stat_->SetString("stat_name", key);
  stat_->SetString("stat_value", "Uninitialized");
  stat_->SetBoolean("is_valid", false);
  section->Append(stat_);
}

void StringSyncStat::SetValue(const std::string& value) {
  stat_->SetString("stat_value", value);
  stat_->SetBoolean("is_valid", true);
}

void StringSyncStat::SetValue(const string16& value) {
  stat_->SetString("stat_value", value);
  stat_->SetBoolean("is_valid", true);
}

class BoolSyncStat {
 public:
  BoolSyncStat(ListValue* section, const std::string& key);
  void SetValue(bool value);

 private:
  // Owned by the |section| passed in during construction.
  DictionaryValue* stat_;
};

BoolSyncStat::BoolSyncStat(ListValue* section, const std::string& key) {
  stat_ = new DictionaryValue();
  stat_->SetString("stat_name", key);
  stat_->SetBoolean("stat_value", false);
  stat_->SetBoolean("is_valid", false);
  section->Append(stat_);
}

void BoolSyncStat::SetValue(bool value) {
  stat_->SetBoolean("stat_value", value);
  stat_->SetBoolean("is_valid", true);
}

class IntSyncStat {
 public:
  IntSyncStat(ListValue* section, const std::string& key);
  void SetValue(int value);

 private:
  // Owned by the |section| passed in during construction.
  DictionaryValue* stat_;
};

IntSyncStat::IntSyncStat(ListValue* section, const std::string& key) {
  stat_ = new DictionaryValue();
  stat_->SetString("stat_name", key);
  stat_->SetInteger("stat_value", 0);
  stat_->SetBoolean("is_valid", false);
  section->Append(stat_);
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

std::string GetTimeStr(base::Time time, const std::string& default_msg) {
  std::string time_str;
  if (time.is_null())
    time_str = default_msg;
  else
    time_str = syncer::GetTimeDebugString(time);
  return time_str;
}

}  // namespace

namespace sync_ui_util {

// This function both defines the structure of the message to be returned and
// its contents.  Most of the message consists of simple fields in about:sync
// which are grouped into sections and populated with the help of the SyncStat
// classes defined above.
scoped_ptr<DictionaryValue> ConstructAboutInformation(
    ProfileSyncService* service) {
  scoped_ptr<DictionaryValue> about_info(new DictionaryValue());
  ListValue* stats_list = new ListValue();  // 'details': A list of sections.

  // The following lines define the sections and their fields.  For each field,
  // a class is instantiated, which allows us to reference the fields in
  // 'setter' code later on in this function.
  ListValue* section_summary = AddSection(stats_list, "Summary");
  StringSyncStat summary_string(section_summary, "Summary");

  ListValue* section_version = AddSection(stats_list, "Version Info");
  StringSyncStat client_version(section_version, "Client Version");
  StringSyncStat server_url(section_version, "Server URL");

  ListValue* section_credentials = AddSection(stats_list, kCredentialsTitle);
  StringSyncStat sync_id(section_credentials, "Sync Client ID");
  StringSyncStat invalidator_id(section_credentials, "Invalidator Client ID");
  StringSyncStat username(section_credentials, "Username");
  BoolSyncStat is_token_available(section_credentials, "Sync Token Available");

  ListValue* section_local = AddSection(stats_list, "Local State");
  StringSyncStat last_synced(section_local, "Last Synced");
  BoolSyncStat is_setup_complete(section_local,
                                 "Sync First-Time Setup Complete");
  BoolSyncStat is_backend_initialized(section_local,
                                      "Sync Backend Initialized");
  BoolSyncStat is_syncing(section_local, "Syncing");

  ListValue* section_network = AddSection(stats_list, "Network");
  BoolSyncStat is_throttled(section_network, "Throttled");
  StringSyncStat retry_time(section_network, "Retry time (maybe stale)");
  BoolSyncStat are_notifications_enabled(section_network,
                                         "Notifications Enabled");

  ListValue* section_encryption = AddSection(stats_list, "Encryption");
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
  StringSyncStat passphrase_type(section_encryption,
                                 "Passphrase Type");
  StringSyncStat passphrase_time(section_encryption,
                                 "Passphrase Time");

  ListValue* section_last_session = AddSection(
      stats_list, "Status from Last Completed Session");
  StringSyncStat session_source(section_last_session, "Sync Source");
  StringSyncStat get_key_result(section_last_session, "GetKey Step Result");
  StringSyncStat download_result(section_last_session, "Download Step Result");
  StringSyncStat commit_result(section_last_session, "Commit Step Result");

  ListValue* section_counters = AddSection(stats_list, "Running Totals");
  IntSyncStat notifications_received(section_counters,
                                     "Notifications Received");
  IntSyncStat empty_get_updates(section_counters, "Cycles Without Updates");
  IntSyncStat non_empty_get_updates(section_counters, "Cycles With Updated");
  IntSyncStat sync_cycles_without_commits(section_counters,
                                          "Cycles Without Commits");
  IntSyncStat sync_cycles_with_commits(section_counters, "Cycles With Commits");
  IntSyncStat useless_sync_cycles(section_counters,
                           "Cycles Without Commits or Updates");
  IntSyncStat useful_sync_cycles(section_counters,
                                 "Cycles With Commit or Update");
  IntSyncStat updates_received(section_counters, "Updates Downloaded");
  IntSyncStat tombstone_updates(section_counters, "Tombstone Updates");
  IntSyncStat reflected_updates(section_counters, "Reflected Updates");
  IntSyncStat successful_commits(section_counters, "Successful Commits");
  IntSyncStat conflicts_resolved_local_wins(section_counters,
                                     "Conflicts Resolved: Client Wins");
  IntSyncStat conflicts_resolved_server_wins(section_counters,
                                      "Conflicts Resolved: Server Wins");

  ListValue *section_this_cycle = AddSection(stats_list,
                                             "Transient Counters (this cycle)");
  IntSyncStat encryption_conflicts(section_this_cycle, "Encryption Conflicts");
  IntSyncStat hierarchy_conflicts(section_this_cycle, "Hierarchy Conflicts");
  IntSyncStat server_conflicts(section_this_cycle, "Server Conflicts");
  IntSyncStat committed_items(section_this_cycle, "Committed Items");
  IntSyncStat updates_remaining(section_this_cycle, "Updates Remaining");

  ListValue* section_that_cycle = AddSection(
      stats_list, "Transient Counters (last cycle of last completed session)");
  IntSyncStat updates_downloaded(section_that_cycle, "Updates Downloaded");
  IntSyncStat committed_count(section_that_cycle, "Committed Count");
  IntSyncStat entries(section_that_cycle, "Entries");

  ListValue* section_nudge_info = AddSection(
      stats_list, "Nudge Source Counters");
  IntSyncStat nudge_source_notification(
      section_nudge_info, "Server Invalidations");
  IntSyncStat nudge_source_local(section_nudge_info, "Local Changes");
  IntSyncStat nudge_source_local_refresh(section_nudge_info, "Local Refreshes");

  // This list of sections belongs in the 'details' field of the returned
  // message.
  about_info->Set(kDetailsKey, stats_list);

  // Populate all the fields we declared above.
  client_version.SetValue(GetVersionString());

  if (!service) {
    summary_string.SetValue("Sync service does not exist");
    return about_info.Pass();
  }

  syncer::SyncStatus full_status;
  bool is_status_valid = service->QueryDetailedSyncStatus(&full_status);
  bool sync_initialized = service->sync_initialized();
  const syncer::sessions::SyncSessionSnapshot& snapshot =
      sync_initialized ?
      service->GetLastSessionSnapshot() :
      syncer::sessions::SyncSessionSnapshot();

  if (is_status_valid)
    summary_string.SetValue(service->QuerySyncStatusSummary());

  server_url.SetValue(service->sync_service_url().spec());

  if (is_status_valid && !full_status.sync_id.empty())
    sync_id.SetValue(full_status.sync_id);
  if (is_status_valid && !full_status.invalidator_client_id.empty())
    invalidator_id.SetValue(full_status.invalidator_client_id);
  if (service->signin())
    username.SetValue(service->signin()->GetAuthenticatedUsername());
  is_token_available.SetValue(service->IsSyncTokenAvailable());

  last_synced.SetValue(service->GetLastSyncedTimeString());
  is_setup_complete.SetValue(service->HasSyncSetupCompleted());
  is_backend_initialized.SetValue(sync_initialized);
  if (is_status_valid) {
    is_syncing.SetValue(full_status.syncing);
    retry_time.SetValue(GetTimeStr(full_status.retry_time,
        "Scheduler is not in backoff or throttled"));
  }

  if (snapshot.is_initialized())
    is_throttled.SetValue(snapshot.is_silenced());
  if (is_status_valid) {
    are_notifications_enabled.SetValue(
        full_status.notifications_enabled);
  }

  if (sync_initialized) {
    is_using_explicit_passphrase.SetValue(
        service->IsUsingSecondaryPassphrase());
    is_passphrase_required.SetValue(service->IsPassphraseRequired());
    passphrase_time.SetValue(
        GetTimeStr(service->GetExplicitPassphraseTime(), "No Passphrase Time"));
  }
  if (is_status_valid) {
    is_cryptographer_ready.SetValue(full_status.cryptographer_ready);
    has_pending_keys.SetValue(full_status.crypto_has_pending_keys);
    encrypted_types.SetValue(
        ModelTypeSetToString(full_status.encrypted_types));
    has_keystore_key.SetValue(full_status.has_keystore_key);
    keystore_migration_time.SetValue(
        GetTimeStr(full_status.keystore_migration_time, "Not Migrated"));
    passphrase_type.SetValue(
        PassphraseTypeToString(full_status.passphrase_type));
  }

  if (snapshot.is_initialized()) {
    session_source.SetValue(
        syncer::GetUpdatesSourceString(snapshot.source().updates_source));
    get_key_result.SetValue(
        GetSyncerErrorString(
            snapshot.model_neutral_state().last_get_key_result));
    download_result.SetValue(
        GetSyncerErrorString(
            snapshot.model_neutral_state().last_download_updates_result));
    commit_result.SetValue(
        GetSyncerErrorString(
            snapshot.model_neutral_state().commit_result));
  }

  if (is_status_valid) {
    notifications_received.SetValue(full_status.notifications_received);
    empty_get_updates.SetValue(full_status.empty_get_updates);
    non_empty_get_updates.SetValue(full_status.nonempty_get_updates);
    sync_cycles_without_commits.SetValue(
        full_status.sync_cycles_without_commits);
    sync_cycles_with_commits.SetValue(
        full_status.sync_cycles_with_commits);
    useless_sync_cycles.SetValue(full_status.useless_sync_cycles);
    useful_sync_cycles.SetValue(full_status.useful_sync_cycles);
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
    updates_remaining.SetValue(full_status.updates_available);
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
      full_status.sync_protocol_error.error_type != syncer::UNKNOWN_ERROR &&
      full_status.sync_protocol_error.error_type != syncer::SYNC_SUCCESS;

  about_info->SetBoolean("actionable_error_detected",
                         actionable_error_detected);

  // NOTE: We won't bother showing any of the following values unless
  // actionable_error_detected is set.

  ListValue* actionable_error = new ListValue();
  about_info->Set("actionable_error", actionable_error);

  StringSyncStat error_type(actionable_error, "Error Type");
  StringSyncStat action(actionable_error, "Action");
  StringSyncStat url(actionable_error, "URL");
  StringSyncStat description(actionable_error, "Error Description");

  if (actionable_error_detected) {
    error_type.SetValue(syncer::GetSyncErrorTypeString(
            full_status.sync_protocol_error.error_type));
    action.SetValue(syncer::GetClientActionString(
            full_status.sync_protocol_error.action));
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
        "Unrecoverable error detected at " + location_str +
        ": " + service->unrecoverable_error_message();
    about_info->SetString("unrecoverable_error_message",
                       unrecoverable_error_message);
  }

  about_info->Set("type_status", service->GetTypeStatusMap());

  return about_info.Pass();
}

}  // namespace sync_ui_util
