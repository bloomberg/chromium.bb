// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DOCUMENT_ENTRY_CONVERSION_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DOCUMENT_ENTRY_CONVERSION_H_

#include <string>

#include "base/memory/scoped_ptr.h"

class GURL;

namespace gdata {

class DocumentEntry;
class DriveEntryProto;

// Converts a DocumentEntry into a DriveEntryProto.
DriveEntryProto ConvertDocumentEntryToDriveEntryProto(
    const DocumentEntry& document_entry);

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DOCUMENT_ENTRY_CONVERSION_H_
