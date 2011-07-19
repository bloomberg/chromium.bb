// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <windows.h>
#include <shellapi.h>

#include "base/message_loop.h"
#include "base/threading/thread.h"
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

  ICONINFO icon_info = { 0 };
  BITMAP bitmap_info = { 0 };

  BOOL r = ::GetIconInfo(file_info.hIcon, &icon_info);
  DCHECK(r);
  r = ::GetObject(icon_info.hbmMask, sizeof(bitmap_info), &bitmap_info);
  DCHECK(r);

  gfx::Size icon_size(bitmap_info.bmWidth, bitmap_info.bmHeight);
  image_.reset(new gfx::Image(
      IconUtil::CreateSkBitmapFromHICON(file_info.hIcon, icon_size)));

  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoader::NotifyDelegate));
}
