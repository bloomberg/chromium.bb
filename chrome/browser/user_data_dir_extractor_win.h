// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_DATA_DIR_EXTRACTOR_WIN_H_
#define CHROME_BROWSER_USER_DATA_DIR_EXTRACTOR_WIN_H_

#include "base/callback.h"
#include "base/files/file_path.h"

namespace chrome {

typedef base::Callback<base::FilePath()> GetUserDataDirCallback;

// Tests can call this to install a custom callback to be called when
// GetUserDataDir() is invoked.
void InstallCustomGetUserDataDirCallbackForTest(
    GetUserDataDirCallback* callback);

}  // namespace chrome

#endif  // CHROME_BROWSER_USER_DATA_DIR_EXTRACTOR_WIN_H_
