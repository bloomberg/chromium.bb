// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DWRITE_FONT_PLATFORM_WIN_H_
#define CONTENT_COMMON_DWRITE_FONT_PLATFORM_WIN_H_

#include "base/files/file.h"
#include "content/common/content_export.h"

struct IDWriteFactory;
struct IDWriteFontCollection;

namespace content {

// Function returns custom font collection in terms of IDWriteFontCollection.
// This function maintains singleton instance of font collection and returns
// it on repeated calls.
CONTENT_EXPORT IDWriteFontCollection* GetCustomFontCollection(
    IDWriteFactory* factory);

// Build and load is to accomplish both tasks of creating a font cache within
// specified file and then loading it, where loading means creating
// readonly shared memory mapping. As this function need to iterate through all
// available fonts in the system, it may take a while.
CONTENT_EXPORT bool BuildAndLoadFontCache(const base::FilePath& file);

// Loads font cache from file. This is supposed to be used from browser
// side where loading means creating readonly shared memory file mapping so that
// renderers can read from it.
CONTENT_EXPORT bool LoadFontCache(const base::FilePath& path);

// Added in header mainly for unittest
CONTENT_EXPORT bool ValidateFontCacheFile(base::File* file);

}  // namespace content

#endif  // CONTENT_COMMON_DWRITE_FONT_PLATFORM_WIN_H_
