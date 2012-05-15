// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "ui/gfx/size.h"

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
  SHFILEINFO file_info = { 0 };
  if (!SHGetFileInfo(group_.c_str(), FILE_ATTRIBUTE_NORMAL, &file_info,
                     sizeof(SHFILEINFO),
                     SHGFI_ICON | size | SHGFI_USEFILEATTRIBUTES))
    return;

  scoped_ptr<SkBitmap> bitmap(IconUtil::CreateSkBitmapFromHICON(
      file_info.hIcon));
  image_.reset(new gfx::Image(*bitmap));
  DestroyIcon(file_info.hIcon);
  target_message_loop_->PostTask(FROM_HERE,
      base::Bind(&IconLoader::NotifyDelegate, this));
}
