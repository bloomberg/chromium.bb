// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_FILE_METADATA_H_
#define CHROME_BROWSER_COCOA_FILE_METADATA_H_
#pragma once

class FilePath;
class GURL;

namespace file_metadata {

// Adds origin metadata to the file.
// |source| should be the source URL for the download, and |referrer| should be
// the URL the user initiated the download from.
void AddOriginMetadataToFile(const FilePath& file, const GURL& source,
                             const GURL& referrer);

// Adds quarantine metadata to the file, assuming it has already been
// quarantined by the OS.
// |source| should be the source URL for the download, and |referrer| should be
// the URL the user initiated the download from.
void AddQuarantineMetadataToFile(const FilePath& file, const GURL& source,
                                 const GURL& referrer);

}  // namespace file_metadata

#endif  // CHROME_BROWSER_COCOA_FILE_METADATA_H_
