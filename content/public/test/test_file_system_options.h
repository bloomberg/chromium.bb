// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_OPTIONS_H_
#define CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_OPTIONS_H_

#include "webkit/browser/fileapi/file_system_options.h"

namespace content {

// Returns Filesystem options for incognito mode.
fileapi::FileSystemOptions CreateIncognitoFileSystemOptions();

// Returns Filesystem options that allow file access.
fileapi::FileSystemOptions CreateAllowFileAccessOptions();

// Returns Filesystem options that disallow file access.
fileapi::FileSystemOptions CreateDisallowFileAccessOptions();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_OPTIONS_H_
