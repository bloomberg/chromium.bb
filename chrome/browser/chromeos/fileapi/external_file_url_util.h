// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_

#include "base/callback_forward.h"
#include "storage/common/fileapi/file_system_types.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

namespace chromeos {

// Returns whether the external file URL is provided for the |type| or not.
bool IsExternalFileURLType(storage::FileSystemType type);

// Obtains the external file url formatted as "externalfile:<path>" from file
// path. Returns empty URL if the file system does not provide the external file
// URL.
GURL FileSystemURLToExternalFileURL(
    const storage::FileSystemURL& file_system_url);

// Converts a externalfile: URL back to a virtual path of FileSystemURL.
base::FilePath ExternalFileURLToVirtualPath(const GURL& url);

// Converts a virtual path of FileSystemURL to an externalfile: URL.
GURL VirtualPathToExternalFileURL(const base::FilePath& virtual_path);

// Obtains external file URL (e.g. external:drive/root/sample.txt) from file
// path (e.g. /special/drive-xxx/root/sample.txt), if the |path| points an
// external location (drive, MTP, or FSP). Otherwise, it returns empty URL.
GURL CreateExternalFileURLFromPath(Profile* profile,
                                   const base::FilePath& path);

// Obtains, from a file path (e.g. /special/drive-xxx/root/sample.txt), an
// external file URL (e.g. external:drive/root/sample.txt), if the |path| points
// an external location (drive, MTP, or FSP), or its hosted file URL if |path|
// refers to a hosted doc (e.g. gdoc) in DriveFS. If neither condition applies,
// |callback| is invoked with an empty GURL.
void ResolveExternalFileUrlFromPath(Profile* profile,
                                    const base::FilePath& path,
                                    base::OnceCallback<void(GURL)> callback);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_
