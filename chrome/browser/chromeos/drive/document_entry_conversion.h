// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DOCUMENT_ENTRY_CONVERSION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DOCUMENT_ENTRY_CONVERSION_H_

#include <string>

#include "base/memory/scoped_ptr.h"

class GURL;

namespace gdata {
class DocumentEntry;
}

namespace drive {

class DriveEntryProto;

// Converts a gdata::DocumentEntry into a DriveEntryProto.
DriveEntryProto ConvertDocumentEntryToDriveEntryProto(
    const gdata::DocumentEntry& document_entry);

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DOCUMENT_ENTRY_CONVERSION_H_
