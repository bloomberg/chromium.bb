// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_PROCESSOR_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_PROCESSOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "chrome/browser/chromeos/gdata/drive_file_formats.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace gdata {

class DriveDirectory;
class DriveEntry;
class DriveResourceMetadata;

typedef std::map<std::string /* resource_id */, DriveEntry*>
    FileResourceIdMap;

// Struct used to record UMA stats with FeedToFileResourceMap().
struct FeedToFileResourceMapUmaStats {
  FeedToFileResourceMapUmaStats();
  ~FeedToFileResourceMapUmaStats();

  typedef std::map<DriveEntryKind, int> EntryKindToCountMap;
  typedef std::map<DriveFileFormat, int> FileFormatToCountMap;
  int num_regular_files;
  int num_hosted_documents;
  EntryKindToCountMap num_files_with_entry_kind;
  FileFormatToCountMap num_files_with_file_format;
};

// GDataWapiFeedProcessor is used to process feeds from WAPI (codename for
// Documents List API).
class GDataWapiFeedProcessor {
 public:
  explicit GDataWapiFeedProcessor(DriveResourceMetadata* resource_metadata);
  ~GDataWapiFeedProcessor();

  // Applies the documents feeds to the file system using |resource_metadata_|.
  //
  // |start_changestamp| determines the type of feed to process. The value is
  // set to zero for the root feeds, every other value is for the delta feeds.
  //
  // In the case of processing the root feeds |root_feed_changestamp| is used
  // as its initial changestamp value. The value comes from
  // AccountMetadataFeed.
  DriveFileError ApplyFeeds(const ScopedVector<DocumentFeed>& feed_list,
                            int64 start_changestamp,
                            int64 root_feed_changestamp,
                            std::set<FilePath>* changed_dirs);

  // Converts list of document feeds from collected feeds into
  // FileResourceIdMap.
  DriveFileError FeedToFileResourceMap(
    const ScopedVector<DocumentFeed>& feed_list,
    FileResourceIdMap* file_map,
    int64* feed_changestamp,
    FeedToFileResourceMapUmaStats* uma_stats);

 private:
  // Updates UMA histograms about file counts.
  void UpdateFileCountUmaHistograms(
      const FeedToFileResourceMapUmaStats& uma_stats) const;

  // Applies the pre-processed feed from |file_map| map onto the file system.
  // All entries in |file_map| will be erased (i.e. the map becomes empty),
  // and values are deleted.
  void ApplyFeedFromFileUrlMap(bool is_delta_feed,
                               int64 feed_changestamp,
                               FileResourceIdMap* file_map,
                               std::set<FilePath>* changed_dirs);

  // Helper function for adding new |file| from the feed into |directory|. It
  // checks the type of file and updates |changed_dirs| if this file adding
  // operation needs to raise directory notification update. If file is being
  // added to |orphaned_resources| such notifications are not raised since
  // we ignore such files and don't add them to the file system now.
  static void AddEntryToDirectoryAndCollectChangedDirectories(
      DriveEntry* entry,
      DriveDirectory* directory,
      DriveResourceMetadata* orphaned_resources,
      std::set<FilePath>* changed_dirs);

  // Helper function for removing |entry| from |directory|. If |entry| is a
  // directory too, it will collect all its children file paths into
  // |changed_dirs| as well.
  static void RemoveEntryFromDirectoryAndCollectChangedDirectories(
      DriveDirectory* directory,
      DriveEntry* entry,
      std::set<FilePath>* changed_dirs);

  // Finds directory where new |file| should be added to during feed processing.
  // |orphaned_entries_dir| collects files/dirs that don't have a parent in
  // either locally cached file system or in this new feed.
  DriveDirectory* FindDirectoryForNewEntry(
      DriveEntry* new_entry,
      const FileResourceIdMap& file_map,
      DriveResourceMetadata* orphaned_ressources);

  DriveResourceMetadata* resource_metadata_;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(GDataWapiFeedProcessor);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_PROCESSOR_H_
