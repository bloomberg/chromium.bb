// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/dwrite_font_cache_win.h"

#include "base/files/file_path.h"
#include "content/common/dwrite_font_platform_win.h"
#include "content/public/browser/browser_thread.h"

namespace content {

bool InitializeFontCache(const base::FilePath& path) {
  if (!LoadFontCache(path)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE, base::Bind(
            base::IgnoreResult(&content::BuildAndLoadFontCache),
            path));
  }
  return true;
}

}  // namespace content
