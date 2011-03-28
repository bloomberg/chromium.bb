// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/session_state.h"

#include <string>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace sessions {
namespace {

using test::ExpectBooleanValue;
using test::ExpectDictionaryValue;
using test::ExpectIntegerValue;
using test::ExpectListValue;
using test::ExpectStringValue;

class SessionStateTest : public testing::Test {};

TEST_F(SessionStateTest, SyncSourceInfoToValue) {
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source =
      sync_pb::GetUpdatesCallerInfo::PERIODIC;
  syncable::ModelTypePayloadMap types;
  types[syncable::PREFERENCES] = "preferencespayload";
  types[syncable::EXTENSIONS] = "";
  scoped_ptr<DictionaryValue> expected_types_value(
      syncable::ModelTypePayloadMapToValue(types));

  SyncSourceInfo source_info(updates_source, types);

  scoped_ptr<DictionaryValue> value(source_info.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectStringValue("PERIODIC", *value, "updatesSource");
  ExpectDictionaryValue(*expected_types_value, *value, "types");
}

TEST_F(SessionStateTest, SyncerStatusToValue) {
  SyncerStatus status;
  status.invalid_store = true;
  status.syncer_stuck = false;
  status.syncing = true;
  status.num_successful_commits = 5;
  status.num_successful_bookmark_commits = 10;
  status.num_updates_downloaded_total = 100;
  status.num_tombstone_updates_downloaded_total = 200;

  scoped_ptr<DictionaryValue> value(status.ToValue());
  EXPECT_EQ(7u, value->size());
  ExpectBooleanValue(status.invalid_store, *value, "invalidStore");
  ExpectBooleanValue(status.syncer_stuck, *value, "syncerStuck");
  ExpectBooleanValue(status.syncing, *value, "syncing");
  ExpectIntegerValue(status.num_successful_commits,
                     *value, "numSuccessfulCommits");
  ExpectIntegerValue(status.num_successful_bookmark_commits,
                     *value, "numSuccessfulBookmarkCommits");
  ExpectIntegerValue(status.num_updates_downloaded_total,
                     *value, "numUpdatesDownloadedTotal");
  ExpectIntegerValue(status.num_tombstone_updates_downloaded_total,
                     *value, "numTombstoneUpdatesDownloadedTotal");
}

TEST_F(SessionStateTest, ErrorCountersToValue) {
  ErrorCounters counters;
  counters.num_conflicting_commits = 1;
  counters.consecutive_transient_error_commits = 5;
  counters.consecutive_errors = 3;

  scoped_ptr<DictionaryValue> value(counters.ToValue());
  EXPECT_EQ(3u, value->size());
  ExpectIntegerValue(counters.num_conflicting_commits,
                     *value, "numConflictingCommits");
  ExpectIntegerValue(counters.consecutive_transient_error_commits,
                     *value, "consecutiveTransientErrorCommits");
  ExpectIntegerValue(counters.consecutive_errors,
                     *value, "consecutiveErrors");
}

TEST_F(SessionStateTest, DownloadProgressMarkersToValue) {
  std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    std::string marker(i, i);
    download_progress_markers[i] = marker;
  }

  scoped_ptr<DictionaryValue> value(
      DownloadProgressMarkersToValue(download_progress_markers));
  EXPECT_EQ(syncable::MODEL_TYPE_COUNT - syncable::FIRST_REAL_MODEL_TYPE,
            static_cast<int>(value->size()));
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    std::string marker(i, i);
    std::string expected_value;
    EXPECT_TRUE(base::Base64Encode(marker, &expected_value));
    ExpectStringValue(expected_value,
                      *value, syncable::ModelTypeToString(model_type));
  }
}

TEST_F(SessionStateTest, SyncSessionSnapshotToValue) {
  SyncerStatus syncer_status;
  syncer_status.num_successful_commits = 500;
  scoped_ptr<DictionaryValue> expected_syncer_status_value(
      syncer_status.ToValue());

  ErrorCounters errors;
  errors.num_conflicting_commits = 250;
  scoped_ptr<DictionaryValue> expected_errors_value(
      errors.ToValue());

  const int kNumServerChangesRemaining = 105;
  const bool kIsShareUsable = true;

  syncable::ModelTypeBitSet initial_sync_ended;
  initial_sync_ended.set(syncable::BOOKMARKS);
  initial_sync_ended.set(syncable::PREFERENCES);
  scoped_ptr<ListValue> expected_initial_sync_ended_value(
      syncable::ModelTypeBitSetToValue(initial_sync_ended));

  std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
  download_progress_markers[syncable::BOOKMARKS] = "test";
  download_progress_markers[syncable::APPS] = "apps";
  scoped_ptr<DictionaryValue> expected_download_progress_markers_value(
      DownloadProgressMarkersToValue(download_progress_markers));

  const bool kHasMoreToSync = false;
  const bool kIsSilenced = true;
  const int kUnsyncedCount = 1053;
  const int kNumConflictingUpdates = 1055;
  const bool kDidCommitItems = true;

  SyncSourceInfo source;
  scoped_ptr<DictionaryValue> expected_source_value(source.ToValue());

  SyncSessionSnapshot snapshot(syncer_status,
                               errors,
                               kNumServerChangesRemaining,
                               kIsShareUsable,
                               initial_sync_ended,
                               download_progress_markers,
                               kHasMoreToSync,
                               kIsSilenced,
                               kUnsyncedCount,
                               kNumConflictingUpdates,
                               kDidCommitItems,
                               source);
  scoped_ptr<DictionaryValue> value(snapshot.ToValue());
  EXPECT_EQ(12u, value->size());
  ExpectDictionaryValue(*expected_syncer_status_value, *value,
                        "syncerStatus");
  ExpectDictionaryValue(*expected_errors_value, *value, "errors");
  ExpectIntegerValue(kNumServerChangesRemaining, *value,
                     "numServerChangesRemaining");
  ExpectBooleanValue(kIsShareUsable, *value, "isShareUsable");
  ExpectListValue(*expected_initial_sync_ended_value, *value,
                  "initialSyncEnded");
  ExpectDictionaryValue(*expected_download_progress_markers_value,
                        *value, "downloadProgressMarkers");
  ExpectBooleanValue(kHasMoreToSync, *value, "hasMoreToSync");
  ExpectBooleanValue(kIsSilenced, *value, "isSilenced");
  ExpectIntegerValue(kUnsyncedCount, *value, "unsyncedCount");
  ExpectIntegerValue(kNumConflictingUpdates, *value,
                     "numConflictingUpdates");
  ExpectBooleanValue(kDidCommitItems, *value,
                     "didCommitItems");
  ExpectDictionaryValue(*expected_source_value, *value, "source");
}

}  // namespace
}  // namespace sessions
}  // namespace browser_sync
