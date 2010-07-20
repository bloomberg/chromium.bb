// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_VERSION_INFO_H_
#define CHROME_COMMON_CHROME_VERSION_INFO_H_

class FileVersionInfo;

namespace chrome {

// Creates a FileVersionInfo for the app, Chrome. Returns NULL in case of
// error. The returned object should be deleted when you are done with it.
FileVersionInfo* GetChromeVersionInfo();

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_VERSION_INFO_H_
