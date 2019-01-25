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
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "base/thread_annotations.h"
#include "media/base/test_data_util.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

constexpr const char* kTestFilename = "pixel-1280x720.jpg";
constexpr const char* kExpectedMd5SumI420 = "6e9e1716073c9a9a1282e3f0e0dab743";
constexpr const char* kExpectedMd5SumYUYV = "ff313a6aedbc4e157561e5c2d5c2e079";

constexpr VAImageFormat kImageFormatI420 = {
    .fourcc = VA_FOURCC_I420,
    .byte_order = VA_LSB_FIRST,
    .bits_per_pixel = 12,
};

constexpr VAImageFormat kImageFormatYUYV = {
    .fourcc = VA_FOURCC('Y', 'U', 'Y', 'V'),
    .byte_order = VA_LSB_FIRST,
    .bits_per_pixel = 16,
};

void LogOnError() {
  LOG(FATAL) << "Oh noes! Decoder failed";
}

uint32_t GetVASurfaceFormat() {
  if (VaapiWrapper::IsImageFormatSupported(kImageFormatI420))
    return VA_RT_FORMAT_YUV420;
  else if (VaapiWrapper::IsImageFormatSupported(kImageFormatYUYV))
    return VA_RT_FORMAT_YUV422;

  LOG(FATAL) << "Neither I420 nor YUY2 is supported.";
  return 0;
}

VAImageFormat GetVAImageFormat() {
  if (VaapiWrapper::IsImageFormatSupported(kImageFormatI420))
    return kImageFormatI420;
  else if (VaapiWrapper::IsImageFormatSupported(kImageFormatYUYV))
    return kImageFormatYUYV;

  LOG(FATAL) << "Neither I420 nor YUY2 is supported.";
  return VAImageFormat{};
}

}  // namespace

class VaapiJpegDecoderTest : public ::testing::Test {
 protected:
  VaapiJpegDecoderTest() {
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (cmd_line && cmd_line->HasSwitch("test_data_path"))
      test_data_path_ = cmd_line->GetSwitchValueASCII("test_data_path");
  }

  void SetUp() override {
    base::RepeatingClosure report_error_cb = base::BindRepeating(&LogOnError);
    wrapper_ = VaapiWrapper::Create(VaapiWrapper::kDecode,
                                    VAProfileJPEGBaseline, report_error_cb);
    ASSERT_TRUE(wrapper_);

    // Load the test data, if not loaded yet.
    if (jpeg_data_.size() == 0) {
      base::FilePath input_file = FindTestDataFilePath(kTestFilename);
      ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data_))
          << "failed to read input data from " << input_file.value();
    }
  }

  void TearDown() override { wrapper_ = nullptr; }

  base::FilePath FindTestDataFilePath(const std::string& file_name);

  bool VerifyDecode(const JpegParseResult& parse_result) const;
  bool Decode(VaapiWrapper* vaapi_wrapper,
              const JpegParseResult& parse_result,
              VASurfaceID va_surface) const;

  base::Lock* GetVaapiWrapperLock() const LOCK_RETURNED(wrapper_->va_lock_) {
    return wrapper_->va_lock_;
  }

  VADisplay GetVaapiWrapperVaDisplay() const
      EXCLUSIVE_LOCKS_REQUIRED(wrapper_->va_lock_) {
    return wrapper_->va_display_;
  }

 protected:
  scoped_refptr<VaapiWrapper> wrapper_;
  std::string jpeg_data_;
  std::string test_data_path_;
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
    const JpegParseResult& parse_result) const {
  gfx::Size size(parse_result.frame_header.coded_width,
                 parse_result.frame_header.coded_height);

  uint32_t va_surface_format = GetVASurfaceFormat();
  VAImageFormat va_image_format = GetVAImageFormat();

  // Depending on the platform, the HW decoder will either convert the image to
  // the I420 format, or use the JPEG's chroma sub-sampling type.
  const char* expected_md5sum = nullptr;
  VideoPixelFormat pixel_format = PIXEL_FORMAT_UNKNOWN;
  if (VaapiWrapper::IsImageFormatSupported(kImageFormatI420)) {
    expected_md5sum = kExpectedMd5SumI420;
    pixel_format = PIXEL_FORMAT_I420;
  } else if (VaapiWrapper::IsImageFormatSupported(kImageFormatYUYV)) {
    expected_md5sum = kExpectedMd5SumYUYV;
    pixel_format = PIXEL_FORMAT_YUY2;
  } else {
    LOG(FATAL) << "Neither I420 nor YUY2 is supported.";
  }

  std::vector<VASurfaceID> va_surfaces;
  if (!wrapper_->CreateContextAndSurfaces(va_surface_format, size, 1,
                                          &va_surfaces)) {
    return false;
  }

  EXPECT_EQ(va_surfaces.size(), 1u);
  if (va_surfaces.size() == 0 ||
      !Decode(wrapper_.get(), parse_result, va_surfaces[0])) {
    LOG(ERROR) << "Decode failed";
    return false;
  }

  auto scoped_image =
      wrapper_->CreateVaImage(va_surfaces[0], &va_image_format, size);
  if (!scoped_image) {
    LOG(ERROR) << "Cannot get VAImage";
    return false;
  }

  EXPECT_TRUE(va_image_format.fourcc == scoped_image->image()->format.fourcc);
  const auto* mem = static_cast<char*>(scoped_image->va_buffer()->data());

  base::StringPiece result(mem, VideoFrame::AllocationSize(pixel_format, size));
  EXPECT_EQ(expected_md5sum, base::MD5String(result));

  return true;
}

bool VaapiJpegDecoderTest::Decode(VaapiWrapper* vaapi_wrapper,
                                  const JpegParseResult& parse_result,
                                  VASurfaceID va_surface) const {
  return VaapiJpegDecoder::DoDecode(vaapi_wrapper, parse_result, va_surface);
}

TEST_F(VaapiJpegDecoderTest, DecodeSuccess) {
  JpegParseResult parse_result;
  ASSERT_TRUE(
      ParseJpegPicture(reinterpret_cast<const uint8_t*>(jpeg_data_.data()),
                       jpeg_data_.size(), &parse_result));

  EXPECT_TRUE(VerifyDecode(parse_result));
}

TEST_F(VaapiJpegDecoderTest, DecodeFail) {
  JpegParseResult parse_result;
  ASSERT_TRUE(
      ParseJpegPicture(reinterpret_cast<const uint8_t*>(jpeg_data_.data()),
                       jpeg_data_.size(), &parse_result));

  // Not supported by VAAPI.
  parse_result.frame_header.num_components = 1;
  parse_result.scan.num_components = 1;

  gfx::Size size(parse_result.frame_header.coded_width,
                 parse_result.frame_header.coded_height);

  std::vector<VASurfaceID> va_surfaces;
  ASSERT_TRUE(wrapper_->CreateContextAndSurfaces(GetVASurfaceFormat(), size, 1,
                                                 &va_surfaces));

  EXPECT_FALSE(Decode(wrapper_.get(), parse_result, va_surfaces[0]));
}

// This test exercises the usual ScopedVAImage lifetime.
TEST_F(VaapiJpegDecoderTest, ScopedVAImage) {
  std::vector<VASurfaceID> va_surfaces;
  const gfx::Size coded_size(64, 64);
  ASSERT_TRUE(wrapper_->CreateContextAndSurfaces(VA_RT_FORMAT_YUV420,
                                                 coded_size, 1, &va_surfaces));
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
