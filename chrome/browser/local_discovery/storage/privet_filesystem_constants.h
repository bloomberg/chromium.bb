// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_CONSTANTS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_CONSTANTS_H_

#include "base/files/file_path.h"

namespace local_discovery {

extern const base::FilePath::CharType kPrivetFilePath[];
extern const char kPrivetTypeStorage[];

extern const char kPrivetListEntries[];
extern const char kPrivetListKeyName[];
extern const char kPrivetListKeySize[];
extern const char kPrivetListKeyType[];
extern const char kPrivetListTypeDir[];

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_CONSTANTS_H_
