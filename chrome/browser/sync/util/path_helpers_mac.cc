// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/port.h"
#include "chrome/browser/sync/util/path_helpers.h"

#ifndef OS_MACOSX
#error Compile this file on Mac only.
#endif

PathString GetFullPath(const PathString& path) {
  // TODO(sync): Not sure what the base of the relative path should be on
  // OS X.
  return path;
}

