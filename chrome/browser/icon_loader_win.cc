// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <windows.h>
#include <shellapi.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

// static
IconGroupID IconLoader::ReadGroupIDFromFilepath(
    const base::FilePath& filepath) {
  base::FilePath::StringType extension = filepath.Extension();
  if (extension != L".exe" && extension != L".dll" && extension != L".ico")
    return extension;
  else
    return filepath.value();
}

void IconLoader::ReadIcon() {
  int size = 0;
  switch (icon_size_) {
    case IconLoader::SMALL:
      size = SHGFI_SMALLICON;
      break;
    case IconLoader::NORMAL:
      size = 0;
      break;
    case IconLoader::LARGE:
      size = SHGFI_LARGEICON;
      break;
    default:
      NOTREACHED();
  }

  image_.reset();

  SHFILEINFO file_info = { 0 };
  if (SHGetFileInfo(group_.c_str(), FILE_ATTRIBUTE_NORMAL, &file_info,
                     sizeof(SHFILEINFO),
                     SHGFI_ICON | size | SHGFI_USEFILEATTRIBUTES)) {
    scoped_ptr<SkBitmap> bitmap(IconUtil::CreateSkBitmapFromHICON(
        file_info.hIcon));
    if (bitmap.get()) {
      gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(*bitmap);
      image_skia.MakeThreadSafe();
      image_.reset(new gfx::Image(image_skia));
      DestroyIcon(file_info.hIcon);
    }
  }

  // Always notify the delegate, regardless of success.
  target_message_loop_->PostTask(FROM_HERE,
      base::Bind(&IconLoader::NotifyDelegate, this));
}
