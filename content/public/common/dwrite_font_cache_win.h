// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_DWRITE_FONT_CACHE_WIN_H_
#define CONTENT_PUBLIC_COMMON_DWRITE_FONT_CACHE_WIN_H_

#include "base/files/file.h"
#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT extern const char kFontCacheSharedSectionName[];

CONTENT_EXPORT bool InitializeFontCache(const base::FilePath& path);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_DWRITE_FONT_CACHE_WIN_H_
