// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_TEST_UTILS_H_
#define CC_TEST_PIXEL_TEST_UTILS_H_

#include "base/files/file_path.h"

class SkBitmap;

namespace cc {

// Encodes a bitmap into a PNG and write to disk. Returns true on success. The
// parent directory does not have to exist.
bool WritePNGFile(const SkBitmap& bitmap, const base::FilePath& file_path);

// Reads and decodes a PNG image to a bitmap. Returns true on success. The PNG
// should have been encoded using |gfx::PNGCodec::Encode|.
bool ReadPNGFile(const base::FilePath& file_path, SkBitmap* bitmap);

// Compares with a PNG file on disk, and returns true if it is the same as
// the given image. |ref_img_path| is absolute.
bool IsSameAsPNGFile(const SkBitmap& gen_bmp, base::FilePath ref_img_path);

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_UTILS_H_
