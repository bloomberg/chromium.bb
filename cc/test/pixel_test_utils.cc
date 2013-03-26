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

bool WritePNGFile(const SkBitmap& bitmap, const base::FilePath& file_path) {
  std::vector<unsigned char> png_data;
  const bool discard_transparency = true;
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

bool IsSameAsPNGFile(const SkBitmap& gen_bmp, base::FilePath ref_img_path) {
  SkBitmap ref_bmp;
  if (!ReadPNGFile(ref_img_path, &ref_bmp)) {
    LOG(ERROR) << "Cannot read reference image: " << ref_img_path.value();
    return false;
  }

  if (ref_bmp.width() != gen_bmp.width() ||
      ref_bmp.height() != gen_bmp.height()) {
    LOG(ERROR)
        << "Dimensions do not match (Expected) vs (Actual):"
        << "(" << ref_bmp.width() << "x" << ref_bmp.height()
            << ") vs. "
        << "(" << gen_bmp.width() << "x" << gen_bmp.height() << ")";
    return false;
  }

  // Compare pixels and create a simple diff image.
  int diff_pixels_count = 0;
  SkAutoLockPixels lock_bmp(gen_bmp);
  SkAutoLockPixels lock_ref_bmp(ref_bmp);
  // The reference images were saved with no alpha channel. Use the mask to
  // set alpha to 0.
  uint32_t kAlphaMask = 0x00FFFFFF;
  for (int x = 0; x < gen_bmp.width(); ++x) {
    for (int y = 0; y < gen_bmp.height(); ++y) {
      if ((*gen_bmp.getAddr32(x, y) & kAlphaMask) !=
          (*ref_bmp.getAddr32(x, y) & kAlphaMask)) {
        ++diff_pixels_count;
      }
    }
  }

  if (diff_pixels_count != 0) {
    LOG(ERROR) << "Images differ by pixel count: " << diff_pixels_count;
    return false;
  }

  return true;
}

}  // namespace cc
