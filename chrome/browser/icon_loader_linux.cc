// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/nix/mime_util_xdg.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "webkit/glue/image_decoder.h"

using std::string;

// static
IconGroupID IconLoader::ReadGroupIDFromFilepath(
    const base::FilePath& filepath) {
  return base::nix::GetFileMimeType(filepath);
}

void IconLoader::ReadIcon() {
  int size_pixels = 0;
  switch (icon_size_) {
    case IconLoader::SMALL:
      size_pixels = 16;
      break;
    case IconLoader::NORMAL:
      size_pixels = 32;
      break;
    case IconLoader::LARGE:
      size_pixels = 48;
      break;
    default:
      NOTREACHED();
  }

  base::FilePath filename = base::nix::GetMimeIcon(group_, size_pixels);
  // We don't support SVG icons; this just spams the terminal so fail quickly
  // and don't try to read the file from disk first.
  if (filename.Extension() != ".svg") {
    string icon_data;
    file_util::ReadFileToString(filename, &icon_data);

    webkit_glue::ImageDecoder decoder;
    SkBitmap bitmap;
    bitmap = decoder.Decode(
        reinterpret_cast<const unsigned char*>(icon_data.data()),
        icon_data.length());
    if (!bitmap.empty()) {
      DCHECK_EQ(size_pixels, bitmap.width());
      DCHECK_EQ(size_pixels, bitmap.height());
      gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
      image_skia.MakeThreadSafe();
      image_.reset(new gfx::Image(image_skia));
    } else {
      LOG(WARNING) << "Unsupported file type or load error: "
                   << filename.value();
    }
  }

  target_message_loop_->PostTask(
      FROM_HERE, base::Bind(&IconLoader::NotifyDelegate, this));
}
