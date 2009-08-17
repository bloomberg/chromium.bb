// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility functions for Mac OS X.

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"

namespace download_util {

void AddFileToPasteboard(NSPasteboard* pasteboard, const FilePath& path);

}  // namespace download_util
