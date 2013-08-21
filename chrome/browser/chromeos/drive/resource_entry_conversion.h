// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_ENTRY_CONVERSION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_ENTRY_CONVERSION_H_

namespace base {
struct PlatformFileInfo;
}

namespace google_apis {
class ResourceEntry;
}

namespace drive {

class ResourceEntry;

// Converts a google_apis::ResourceEntry into a drive::ResourceEntry.
// If the conversion succeeded, return true and sets the result to |output|.
// If failed, it returns false and keeps |*output| untouched.
//
// Every entry is guaranteed to have one parent resource ID in ResourceMetadata.
// This requirement is needed to represent contents in Drive as a file system
// tree, and achieved as follows:
//
// 1) Entries without parents are allowed on drive.google.com. These entries are
// collected to "drive/other", and have "drive/other" as the parent.
//
// 2) Entries with multiple parents are allowed on drive.google.com. For these
// entries, the first parent is chosen.
bool ConvertToResourceEntry(const google_apis::ResourceEntry& input,
                            ResourceEntry* output);

// Converts the resource entry to the platform file info.
void ConvertResourceEntryToPlatformFileInfo(const ResourceEntry& entry,
                                            base::PlatformFileInfo* file_info);

// Converts the platform file info and sets it to the .file_info field of
// the resource entry.
void SetPlatformFileInfoToResourceEntry(const base::PlatformFileInfo& file_info,
                                        ResourceEntry* entry);

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_ENTRY_CONVERSION_H_
