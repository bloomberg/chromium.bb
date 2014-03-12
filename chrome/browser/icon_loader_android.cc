// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/icon_loader.h"

// static
IconGroupID IconLoader::ReadGroupIDFromFilepath(
    const base::FilePath& filepath) {
  NOTIMPLEMENTED();
  return 0;
}

bool IconLoader::IsIconMutableFromFilepath(const base::FilePath&) {
  return false;
}

// static
content::BrowserThread::ID IconLoader::ReadIconThreadID() {
  NOTIMPLEMENTED();
  return content::BrowserThread::FILE;
}

void IconLoader::ReadIcon() {
  NOTIMPLEMENTED();
}
