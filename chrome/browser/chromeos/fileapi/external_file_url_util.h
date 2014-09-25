// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_

class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace chromeos {

// Returns the external file resource url formatted as "externalfile:<path>"
GURL FilePathToExternalFileURL(const base::FilePath& path);

// Converts a externalfile: URL back to a path that can be passed to FileSystem.
base::FilePath ExternalFileURLToFilePath(const GURL& url);

// Obtains external file URL (e.g. external:drive/root/sample.txt) from file
// path (e.g. /special/drive-xxx/root/sample.txt), if the |path| points an
// external location (drive, MTP, or FSP). Otherwise, it returns empty URL.
GURL CreateExternalFileURLFromPath(Profile* profile,
                                   const base::FilePath& path);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_
