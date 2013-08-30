// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/nix/mime_util_xdg.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

using std::string;

// static
IconGroupID IconLoader::ReadGroupIDFromFilepath(
    const base::FilePath& filepath) {
  return base::nix::GetFileMimeType(filepath);
}

bool IconLoader::IsIconMutableFromFilepath(const base::FilePath&) {
  return false;
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
  // We don't support SVG or XPM icons; this just spams the terminal so fail
  // quickly and don't try to read the file from disk first.
  if (filename.Extension() != ".svg" &&
      filename.Extension() != ".xpm") {
    string icon_data;
    base::ReadFileToString(filename, &icon_data);

    SkBitmap bitmap;
    bool success = gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(icon_data.data()),
        icon_data.length(),
        &bitmap);
    if (success && !bitmap.empty()) {
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
