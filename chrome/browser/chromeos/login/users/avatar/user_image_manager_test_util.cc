// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_test_util.h"

#include <string>

#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace chromeos {
namespace test {

const char kUserAvatarImage1RelativePath[] = "chromeos/avatar1.jpg";
const char kUserAvatarImage2RelativePath[] = "chromeos/avatar2.jpg";

bool AreImagesEqual(const gfx::ImageSkia& first, const gfx::ImageSkia& second) {
  if (first.width() != second.width() || first.height() != second.height())
    return false;
  const SkBitmap* first_bitmap = first.bitmap();
  const SkBitmap* second_bitmap = second.bitmap();
  if (!first_bitmap && !second_bitmap)
    return true;
  if (!first_bitmap || !second_bitmap)
    return false;

  const size_t size = first_bitmap->getSize();
  if (second_bitmap->getSize() != size)
    return false;

  SkAutoLockPixels first_pixel_lock(*first_bitmap);
  SkAutoLockPixels second_pixel_lock(*second_bitmap);
  uint8_t* first_data = reinterpret_cast<uint8_t*>(first_bitmap->getPixels());
  uint8_t* second_data = reinterpret_cast<uint8_t*>(second_bitmap->getPixels());
  for (size_t i = 0; i < size; ++i) {
    if (first_data[i] != second_data[i])
      return false;
  }
  return true;
}

ImageLoader::ImageLoader(const base::FilePath& path) : path_(path) {
}

ImageLoader::~ImageLoader() {
}

scoped_ptr<gfx::ImageSkia> ImageLoader::Load() {
  std::string image_data;
  ReadFileToString(path_, &image_data);
  scoped_refptr<ImageDecoder> image_decoder = new ImageDecoder(
      this,
      image_data,
      ImageDecoder::ROBUST_JPEG_CODEC);
  image_decoder->Start(base::MessageLoopProxy::current());
  run_loop_.Run();
  return decoded_image_.Pass();
}

void ImageLoader::OnImageDecoded(const ImageDecoder* decoder,
                                 const SkBitmap& decoded_image) {
  decoded_image_.reset(
      new gfx::ImageSkia(gfx::ImageSkiaRep(decoded_image, 1.0f)));
  run_loop_.Quit();
}

void ImageLoader::OnDecodeImageFailed(const ImageDecoder* decoder) {
  run_loop_.Quit();
}

}  // namespace test
}  // namespace chromeos
