// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <va/va.h>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "base/thread_annotations.h"
#include "media/base/test_data_util.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

// This file is supported by the VAAPI, so define expectations on the decode
// result.
constexpr const char* kSupportedFilename = "pixel-1280x720.jpg";
constexpr const char* kExpectedMd5SumI420 = "6e9e1716073c9a9a1282e3f0e0dab743";
constexpr const char* kExpectedMd5SumYUYV = "ff313a6aedbc4e157561e5c2d5c2e079";

// This file is not supported by the VAAPI, so we don't define expectations on
// the decode result.
constexpr const char* kUnsupportedFilename = "pixel-1280x720-grayscale.jpg";

constexpr VAImageFormat kImageFormatI420 = {
    .fourcc = VA_FOURCC_I420,
    .byte_order = VA_LSB_FIRST,
    .bits_per_pixel = 12,
};

}  // namespace

class VaapiJpegDecoderTest : public ::testing::Test {
 protected:
  VaapiJpegDecoderTest() {
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (cmd_line && cmd_line->HasSwitch("test_data_path"))
      test_data_path_ = cmd_line->GetSwitchValueASCII("test_data_path");
  }

  void SetUp() override {
    ASSERT_TRUE(decoder_.Initialize(base::BindRepeating(
        []() { LOG(FATAL) << "Oh noes! Decoder failed"; })));
  }

  base::FilePath FindTestDataFilePath(const std::string& file_name);

  bool VerifyDecode(base::span<const uint8_t> encoded_image);
  std::unique_ptr<ScopedVAImage> Decode(
      base::span<const uint8_t> encoded_image);

  base::Lock* GetVaapiWrapperLock() const
      LOCK_RETURNED(decoder_.vaapi_wrapper_->va_lock_) {
    return decoder_.vaapi_wrapper_->va_lock_;
  }

  VADisplay GetVaapiWrapperVaDisplay() const
      EXCLUSIVE_LOCKS_REQUIRED(decoder_.vaapi_wrapper_->va_lock_) {
    return decoder_.vaapi_wrapper_->va_display_;
  }

 protected:
  std::string test_data_path_;
  VaapiJpegDecoder decoder_;
};

// Find the location of the specified test file. If a file with specified path
// is not found, treat the file as being relative to the test file directory.
// This is either a custom test data path provided by --test_data_path, or the
// default test data path (//media/test/data).
base::FilePath VaapiJpegDecoderTest::FindTestDataFilePath(
    const std::string& file_name) {
  const base::FilePath file_path = base::FilePath(file_name);
  if (base::PathExists(file_path))
    return file_path;
  if (!test_data_path_.empty())
    return base::FilePath(test_data_path_).Append(file_path);
  return GetTestDataFilePath(file_name);
}

bool VaapiJpegDecoderTest::VerifyDecode(
    base::span<const uint8_t> encoded_image) {
  std::unique_ptr<ScopedVAImage> scoped_image = Decode(encoded_image);
  if (!scoped_image) {
    LOG(ERROR) << "Cannot get VAImage";
    return false;
  }

  const char* expected_md5sum = nullptr;
  if (scoped_image->image()->format.fourcc == VA_FOURCC_I420) {
    expected_md5sum = kExpectedMd5SumI420;
  } else if (scoped_image->image()->format.fourcc == VA_FOURCC_YUY2 ||
             scoped_image->image()->format.fourcc ==
                 VA_FOURCC('Y', 'U', 'Y', 'V')) {
    expected_md5sum = kExpectedMd5SumYUYV;
  } else {
    LOG(FATAL) << "Neither I420 nor YUY2 is supported.";
  }

  const auto* mem = static_cast<char*>(scoped_image->va_buffer()->data());
  base::StringPiece result(mem, scoped_image->image()->data_size);
  EXPECT_EQ(expected_md5sum, base::MD5String(result));
  return true;
}

std::unique_ptr<ScopedVAImage> VaapiJpegDecoderTest::Decode(
    base::span<const uint8_t> encoded_image) {
  VaapiJpegDecodeStatus status;
  std::unique_ptr<ScopedVAImage> scoped_image =
      decoder_.DoDecode(encoded_image, &status);
  EXPECT_EQ(!!scoped_image, status == VaapiJpegDecodeStatus::kSuccess);
  return scoped_image;
}

TEST_F(VaapiJpegDecoderTest, DecodeSuccess) {
  base::FilePath input_file = FindTestDataFilePath(kSupportedFilename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  EXPECT_TRUE(VerifyDecode(base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size())));
}

TEST_F(VaapiJpegDecoderTest, DecodeFail) {
  // Not supported by VAAPI.
  base::FilePath input_file = FindTestDataFilePath(kUnsupportedFilename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  EXPECT_FALSE(Decode(base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size())));
}

// This test exercises the usual ScopedVAImage lifetime.
//
// TODO(andrescj): move ScopedVAImage and ScopedVABufferMapping to a separate
// file so that we don't have to use |decoder_.vaapi_wrapper_|. See
// https://crbug.com/924310.
TEST_F(VaapiJpegDecoderTest, ScopedVAImage) {
  std::vector<VASurfaceID> va_surfaces;
  const gfx::Size coded_size(64, 64);
  ASSERT_TRUE(decoder_.vaapi_wrapper_->CreateContextAndSurfaces(
      VA_RT_FORMAT_YUV420, coded_size, 1, &va_surfaces));
  ASSERT_EQ(va_surfaces.size(), 1u);

  std::unique_ptr<ScopedVAImage> scoped_image;
  {
    // On Stoney-Ridge devices the output image format is dependent on the
    // surface format. However when DoDecode() is not called the output image
    // format seems to default to I420. https://crbug.com/828119
    VAImageFormat va_image_format = kImageFormatI420;
    base::AutoLock auto_lock(*GetVaapiWrapperLock());
    scoped_image = std::make_unique<ScopedVAImage>(
        GetVaapiWrapperLock(), GetVaapiWrapperVaDisplay(), va_surfaces[0],
        &va_image_format, coded_size);

    EXPECT_TRUE(scoped_image->image());
    ASSERT_TRUE(scoped_image->IsValid());
    EXPECT_TRUE(scoped_image->va_buffer()->IsValid());
    EXPECT_TRUE(scoped_image->va_buffer()->data());
  }
}

// This test exercises creation of a ScopedVAImage with a bad VASurfaceID.
TEST_F(VaapiJpegDecoderTest, BadScopedVAImage) {
#if DCHECK_IS_ON()
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
#endif

  const std::vector<VASurfaceID> va_surfaces = {VA_INVALID_ID};
  const gfx::Size coded_size(64, 64);

  std::unique_ptr<ScopedVAImage> scoped_image;
  {
    VAImageFormat va_image_format = kImageFormatI420;
    base::AutoLock auto_lock(*GetVaapiWrapperLock());
    scoped_image = std::make_unique<ScopedVAImage>(
        GetVaapiWrapperLock(), GetVaapiWrapperVaDisplay(), va_surfaces[0],
        &va_image_format, coded_size);

    EXPECT_TRUE(scoped_image->image());
    EXPECT_FALSE(scoped_image->IsValid());
#if DCHECK_IS_ON()
    EXPECT_DCHECK_DEATH(scoped_image->va_buffer());
#else
    EXPECT_FALSE(scoped_image->va_buffer());
#endif
  }
}

// This test exercises creation of a ScopedVABufferMapping with bad VABufferIDs.
TEST_F(VaapiJpegDecoderTest, BadScopedVABufferMapping) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  base::AutoLock auto_lock(*GetVaapiWrapperLock());

  // A ScopedVABufferMapping with a VA_INVALID_ID VABufferID is DCHECK()ed.
  EXPECT_DCHECK_DEATH(std::make_unique<ScopedVABufferMapping>(
      GetVaapiWrapperLock(), GetVaapiWrapperVaDisplay(), VA_INVALID_ID));

  // This should not hit any DCHECK() but will create an invalid
  // ScopedVABufferMapping.
  auto scoped_buffer = std::make_unique<ScopedVABufferMapping>(
      GetVaapiWrapperLock(), GetVaapiWrapperVaDisplay(), VA_INVALID_ID - 1);
  EXPECT_FALSE(scoped_buffer->IsValid());
}

}  // namespace media
