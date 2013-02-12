// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_SEARCH_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_SEARCH_METADATA_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"

namespace drive {

// Struct to represent a search result for SearchMetadata().
struct MetadataSearchResult {
  MetadataSearchResult(const base::FilePath& in_path,
                       const DriveEntryProto& in_entry_proto,
                       const std::string& in_highlighted_base_name)
      : path(in_path),
        entry_proto(in_entry_proto),
        highlighted_base_name(in_highlighted_base_name) {
  }

  // The two members are used to create FileEntry object.
  base::FilePath path;
  DriveEntryProto entry_proto;

  // The base name to be displayed in the UI. The parts matched the search
  // query are highlighted with <b> tag. Meta characters are escaped like &lt;
  //
  // Why HTML? we could instead provide matched ranges using pairs of
  // integers, but this is fragile as we'll eventually converting strings
  // from UTF-8 (StringValue in base/values.h uses std::string) to UTF-16
  // when sending strings from C++ to JavaScript.
  //
  // Why <b> instead of <strong>? Because <b> is shorter.
  std::string highlighted_base_name;
};

typedef std::vector<MetadataSearchResult> MetadataSearchResultVector;

// Callback for SearchMetadata(). On success, |error| is DRIVE_FILE_OK, and
// |result| contains the search result.
typedef base::Callback<void(
    DriveFileError error,
    scoped_ptr<MetadataSearchResultVector> result)> SearchMetadataCallback;

// Searches the local resource metadata, and returns the entries
// |at_most_num_matches| that contain |query| in their base names. Search is
// done in a case-insensitive fashion. |callback| must not be null. Must be
// called on UI thread. No entries are returned if |query| is empty.
void SearchMetadata(DriveFileSystemInterface* file_system,
                    const std::string& query,
                    int at_most_num_matches,
                    const SearchMetadataCallback& callback);

// Finds |query| in |text| while ignoring case. Returns true if |query| is
// found. Returns false if |query| is not found, or empty. |highlighted_text|
// will have the original text with matched portions highlighted with <b> tag
// (multiple portions can be highlighted). Meta characters are escaped like
// &lt;. The original contents of |highlighted| will be lost.
bool FindAndHighlight(const std::string& text,
                      const std::string& query,
                      std::string* highlighted_text);

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_SEARCH_METADATA_H_
