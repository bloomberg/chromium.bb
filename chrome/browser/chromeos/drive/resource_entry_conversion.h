// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_ENTRY_CONVERSION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_ENTRY_CONVERSION_H_

namespace google_apis {
class ResourceEntry;
}

namespace drive {

class ResourceEntry;

// Converts a google_apis::ResourceEntry into a drive::ResourceEntry.
ResourceEntry ConvertToResourceEntry(
    const google_apis::ResourceEntry& resource_entry);

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_RESOURCE_ENTRY_CONVERSION_H_
