// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_
#define CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "webkit/fileapi/sandboxed_file_system_context.h"

// Convenient method that returns SandboxedFileSystemContext
// constructed for the browser process.
scoped_refptr<fileapi::SandboxedFileSystemContext> CreateFileSystemContext(
        const FilePath& profile_path, bool is_incognito);

#endif  // CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_
