// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <string>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/test/gtest_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_frame.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/vaapi/vaapi_jpeg_decode_accelerator.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {
namespace {

constexpr const char* kTestFilename = "pixel-1280x720.jpg";
constexpr const char* kExpectedMd5Sum = "6e9e1716073c9a9a1282e3f0e0dab743";

void LogOnError() {
  LOG(FATAL) << "Oh noes! Decoder failed";
}

// Find the location of the specified test file. If a file with specified path
// is not found, treat the path as being relative to the standard test file
// directory.
base::FilePath FindTestDataFilePath(const std::string& file_name) {
  base::FilePath file_path = base::FilePath(file_name);
  return PathExists(file_path) ? file_path : GetTestDataFilePath(file_name);
}

}  // namespace

class VaapiJpegDecodeAcceleratorTest : public ::testing::Test {
 protected:
  VaapiJpegDecodeAcceleratorTest() {}

  void SetUp() override {
    base::RepeatingClosure report_error_cb = base::BindRepeating(&LogOnError);
    wrapper_ = VaapiWrapper::Create(VaapiWrapper::kDecode,
                                    VAProfileJPEGBaseline, report_error_cb);
    ASSERT_TRUE(wrapper_);

    base::FilePath input_file = FindTestDataFilePath(kTestFilename);

    ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data_))
        << "failed to read input data from " << input_file.value();
  }

  void TearDown() override { wrapper_ = nullptr; }

  bool VerifyDecode(const JpegParseResult& parse_result,
                    const std::string& md5sum);
  bool Decode(VaapiWrapper* vaapi_wrapper,
              const JpegParseResult& parse_result,
              VASurfaceID va_surface);

  base::Lock* GetVaapiWrapperLock() const { return wrapper_->va_lock_; }
  VADisplay GetVaapiWrapperVaDisplay() const { return wrapper_->va_display_; }

 protected:
  scoped_refptr<VaapiWrapper> wrapper_;
  std::string jpeg_data_;
};

bool VaapiJpegDecodeAcceleratorTest::VerifyDecode(
    const JpegParseResult& parse_result,
    const std::string& expected_md5sum) {
  gfx::Size size(parse_result.frame_header.coded_width,
                 parse_result.frame_header.coded_height);

  std::vector<VASurfaceID> va_surfaces;
  if (!wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, size, 1, &va_surfaces))
    return false;

  if (!Decode(wrapper_.get(), parse_result, va_surfaces[0])) {
    LOG(ERROR) << "Decode failed";
    return false;
  }

  VAImageFormat format{};
  format.fourcc = VA_FOURCC_I420;
  format.byte_order = VA_LSB_FIRST;
  format.bits_per_pixel = 12;  // 12 for I420

  auto scoped_image = wrapper_->CreateVaImage(va_surfaces[0], &format, size);
  if (!scoped_image) {
    LOG(ERROR) << "Cannot get VAImage";
    return false;
  }

  EXPECT_TRUE(VA_FOURCC_I420 == scoped_image->image()->format.fourcc);
  const auto* mem = static_cast<char*>(scoped_image->va_buffer()->data());

  base::StringPiece result(mem,
                           VideoFrame::AllocationSize(PIXEL_FORMAT_I420, size));
  EXPECT_EQ(expected_md5sum, base::MD5String(result));

  return true;
}

bool VaapiJpegDecodeAcceleratorTest::Decode(VaapiWrapper* vaapi_wrapper,
                                            const JpegParseResult& parse_result,
                                            VASurfaceID va_surface) {
  return VaapiJpegDecodeAccelerator::DoDecode(vaapi_wrapper, parse_result,
                                              va_surface);
}

TEST_F(VaapiJpegDecodeAcceleratorTest, DecodeSuccess) {
  JpegParseResult parse_result;
  ASSERT_TRUE(
      ParseJpegPicture(reinterpret_cast<const uint8_t*>(jpeg_data_.data()),
                       jpeg_data_.size(), &parse_result));

  EXPECT_TRUE(VerifyDecode(parse_result, kExpectedMd5Sum));
}

TEST_F(VaapiJpegDecodeAcceleratorTest, DecodeFail) {
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
  ASSERT_TRUE(
      wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, size, 1, &va_surfaces));

  EXPECT_FALSE(Decode(wrapper_.get(), parse_result, va_surfaces[0]));
}

// This test exercises the usual ScopedVAImage lifetime.
TEST_F(VaapiJpegDecodeAcceleratorTest, ScopedVAImage) {
  std::vector<VASurfaceID> va_surfaces;
  const gfx::Size coded_size(64, 64);
  ASSERT_TRUE(wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, coded_size, 1,
                                       &va_surfaces));
  ASSERT_EQ(va_surfaces.size(), 1u);

  VAImageFormat va_image_format{.fourcc = VA_FOURCC_I420,
                                .byte_order = VA_LSB_FIRST,
                                .bits_per_pixel = 12};
  ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(va_image_format));

  std::unique_ptr<ScopedVAImage> scoped_image;
  {
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
TEST_F(VaapiJpegDecodeAcceleratorTest, BadScopedVAImage) {
  const std::vector<VASurfaceID> va_surfaces = {VA_INVALID_ID};
  const gfx::Size coded_size(64, 64);

  VAImageFormat va_image_format{.fourcc = VA_FOURCC_I420,
                                .byte_order = VA_LSB_FIRST,
                                .bits_per_pixel = 12};
  ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(va_image_format));

  std::unique_ptr<ScopedVAImage> scoped_image;
  {
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
TEST_F(VaapiJpegDecodeAcceleratorTest, BadScopedVABufferMapping) {
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
