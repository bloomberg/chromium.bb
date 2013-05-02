// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_SEARCH_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_SEARCH_METADATA_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace drive {

namespace internal {
class ResourceMetadata;
}  // namespace internal

// Searches the local resource metadata, and returns the entries
// |at_most_num_matches| that contain |query| in their base names. Search is
// done in a case-insensitive fashion. The eligible entries are selected based
// on the given |options|, which is a bit-wise OR of SearchMetadataOptions.
// |callback| must not be null. Must be called on UI thread. Empty |query|
// matches any base name. i.e. returns everything.
void SearchMetadata(internal::ResourceMetadata* resource_metadata,
                    const std::string& query,
                    int search_options,
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
