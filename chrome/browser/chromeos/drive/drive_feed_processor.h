// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FEED_PROCESSOR_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FEED_PROCESSOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"

namespace drive {

class DriveDirectory;
class DriveEntry;
class DriveResourceMetadata;

// DriveFeedProcessor is used to process feeds from WAPI (codename for
// Documents List API).
class DriveFeedProcessor {
 public:
  typedef std::map<std::string /* resource_id */, DriveEntryProto>
      DriveEntryProtoMap;

  // Class used to record UMA stats with FeedToEntryProtoMap().
  class FeedToEntryProtoMapUMAStats;

  explicit DriveFeedProcessor(DriveResourceMetadata* resource_metadata);
  ~DriveFeedProcessor();

  // Applies the documents feeds to the file system using |resource_metadata_|.
  //
  // |start_changestamp| determines the type of feed to process. The value is
  // set to zero for the root feeds, every other value is for the delta feeds.
  //
  // In the case of processing the root feeds |root_feed_changestamp| is used
  // as its initial changestamp value. The value comes from
  // google_apis::AccountMetadataFeed.
  void ApplyFeeds(
      const ScopedVector<google_apis::DocumentFeed>& feed_list,
      int64 start_changestamp,
      int64 root_feed_changestamp,
      std::set<FilePath>* changed_dirs);

  // Converts list of document feeds from collected feeds into a
  // DriveEntryProtoMap. |feed_changestamp| and/or |uma_stats| may be NULL.
  void FeedToEntryProtoMap(
    const ScopedVector<google_apis::DocumentFeed>& feed_list,
    DriveEntryProtoMap* entry_proto_map,
    int64* feed_changestamp,
    FeedToEntryProtoMapUMAStats* uma_stats);

 private:
  typedef std::map<std::string /* resource_id */, DriveEntry*> ResourceMap;

  // Applies the pre-processed feed from |entry_proto_map| onto the filesystem.
  void ApplyEntryProtoMap(const DriveEntryProtoMap& entry_proto_map,
                          bool is_delta_feed,
                          int64 feed_changestamp,
                          std::set<FilePath>* changed_dirs);

  // Helper function for adding new |entry| from the feed into |directory|. It
  // checks the type of file and updates |changed_dirs| if this file adding
  // operation needs to raise directory notification update.
  static void AddEntryToDirectoryAndCollectChangedDirectories(
      DriveEntry* entry,
      DriveDirectory* directory,
      std::set<FilePath>* changed_dirs);

  // Helper function for removing |entry| from its parent. If |entry| is a
  // directory too, it will collect all its children file paths into
  // |changed_dirs| as well.
  static void RemoveEntryFromParentAndCollectChangedDirectories(
      DriveEntry* entry,
      std::set<FilePath>* changed_dirs);

  // Finds directory where new |new_entry| should be added to during feed
  // processing.
  DriveDirectory* FindDirectoryForNewEntry(
      DriveEntry* new_entry,
      const ResourceMap& resource_map);

  DriveResourceMetadata* resource_metadata_;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(DriveFeedProcessor);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FEED_PROCESSOR_H_
