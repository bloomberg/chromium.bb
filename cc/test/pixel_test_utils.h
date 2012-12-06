// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"

class SkBitmap;

namespace cc {
namespace test {

// Encodes a bitmap into a PNG and write to disk. Returns true on success. The
// parent directory does not have to exist.
bool WritePNGFile(const SkBitmap& bitmap, const FilePath& file_path);

// Reads and decodes a PNG image to a bitmap. Returns true on success. The PNG
// should have been encoded using |gfx::PNGCodec::Encode|.
bool ReadPNGFile(const FilePath& file_path, SkBitmap* bitmap);

// Compares with a PNG file on disk, and returns true if it is the same as
// the given image. |ref_img_path| is absolute.
bool IsSameAsPNGFile(const SkBitmap& gen_bmp, FilePath ref_img_path);

}  // namespace test
}  // namespace cc
