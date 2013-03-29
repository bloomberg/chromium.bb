// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_utils.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace cc {

bool WritePNGFile(const SkBitmap& bitmap, const base::FilePath& file_path,
    bool discard_transparency) {
  std::vector<unsigned char> png_data;
  if (gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                        discard_transparency,
                                        &png_data) &&
      file_util::CreateDirectory(file_path.DirName())) {
    char* data = reinterpret_cast<char*>(&png_data[0]);
    int size = static_cast<int>(png_data.size());
    return file_util::WriteFile(file_path, data, size) == size;
  }
  return false;
}

bool ReadPNGFile(const base::FilePath& file_path, SkBitmap* bitmap) {
  DCHECK(bitmap);
  std::string png_data;
  return file_util::ReadFileToString(file_path, &png_data) &&
         gfx::PNGCodec::Decode(reinterpret_cast<unsigned char*>(&png_data[0]),
                               png_data.length(),
                               bitmap);
}

bool MatchesPNGFile(const SkBitmap& gen_bmp, base::FilePath ref_img_path,
                    const PixelComparator& comparator) {
  SkBitmap ref_bmp;
  if (!ReadPNGFile(ref_img_path, &ref_bmp)) {
    LOG(ERROR) << "Cannot read reference image: " << ref_img_path.value();
    return false;
  }

  // Check if images size matches
  if (gen_bmp.width() != ref_bmp.width() ||
      gen_bmp.height() != ref_bmp.height()) {
    LOG(ERROR)
        << "Dimensions do not match! "
        << "Actual: " << gen_bmp.width() << "x" << gen_bmp.height()
        << "; "
        << "Expected: " << ref_bmp.width() << "x" << ref_bmp.height();
    return false;
  }

  // Shortcut for empty images. They are always equal.
  if (gen_bmp.width() == 0 || gen_bmp.height() == 0)
    return true;

  return comparator.Compare(gen_bmp, ref_bmp);
}

}  // namespace cc
