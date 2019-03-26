// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
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
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "base/thread_annotations.h"
#include "media/base/test_data_util.h"
#include "media/base/video_types.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

constexpr const char* kYuv422Filename = "pixel-1280x720.jpg";
constexpr const char* kYuv420Filename = "pixel-1280x720-yuv420.jpg";

struct TestParam {
  const char* filename;
};

const TestParam kTestCases[] = {
    {kYuv422Filename},
    {kYuv420Filename},
};

// Any number above 99.5% should do, experimentally we like a wee higher.
constexpr double kMinSsim = 0.997;

// This file is not supported by the VAAPI, so we don't define expectations on
// the decode result.
constexpr const char* kUnsupportedFilename = "pixel-1280x720-grayscale.jpg";

constexpr VAImageFormat kImageFormatI420 = {
    .fourcc = VA_FOURCC_I420,
    .byte_order = VA_LSB_FIRST,
    .bits_per_pixel = 12,
};

// The size of the minimum coded unit for a YUV 4:2:0 image (both the width and
// the height of the MCU are the same for 4:2:0).
constexpr int k420MCUSize = 16;

// The largest maximum supported surface size we expect a driver to report for
// JPEG decoding.
constexpr gfx::Size kLargestSupportedSize(16 * 1024, 16 * 1024);

// Compares the result of sw decoding |encoded_image| with |decoded_image| using
// SSIM. Returns true if all conversions work and SSIM is above a given
// threshold (kMinSsim), or false otherwise.
bool CompareImages(base::span<const uint8_t> encoded_image,
                   const ScopedVAImage* decoded_image) {
  JpegParseResult parse_result;
  const bool result = ParseJpegPicture(encoded_image.data(),
                                       encoded_image.size(), &parse_result);
  if (!result)
    return false;

  const uint16_t coded_width = parse_result.frame_header.coded_width;
  const uint16_t coded_height = parse_result.frame_header.coded_height;
  if (coded_width != decoded_image->image()->width ||
      coded_height != decoded_image->image()->height) {
    DLOG(ERROR) << "Wrong expected decoded JPEG coded size, " << coded_width
                << "x" << coded_height << " versus VaAPI provided "
                << decoded_image->image()->width << "x"
                << decoded_image->image()->height;
    return false;
  }

  const uint16_t width = parse_result.frame_header.visible_width;
  const uint16_t height = parse_result.frame_header.visible_height;
  const uint16_t even_width = (width + 1) / 2;
  const uint16_t even_height = (height + 1) / 2;

  auto ref_y = std::make_unique<uint8_t[]>(width * height);
  auto ref_u = std::make_unique<uint8_t[]>(even_width * even_height);
  auto ref_v = std::make_unique<uint8_t[]>(even_width * even_height);

  const int conversion_result = libyuv::ConvertToI420(
      encoded_image.data(), encoded_image.size(), ref_y.get(), width,
      ref_u.get(), even_width, ref_v.get(), even_width, 0, 0, width, height,
      width, height, libyuv::kRotate0, libyuv::FOURCC_MJPG);
  if (conversion_result != 0) {
    DLOG(ERROR) << "libyuv conversion error";
    return false;
  }

  const uint32_t va_fourcc = decoded_image->image()->format.fourcc;
  if (!(va_fourcc == VA_FOURCC_I420 || va_fourcc == VA_FOURCC_YUY2 ||
        va_fourcc == VA_FOURCC('Y', 'U', 'Y', 'V'))) {
    DLOG(ERROR) << "Not supported FourCC: " << FourccToString(va_fourcc);
    return false;
  }
  const uint32_t libyuv_fourcc =
      (va_fourcc == VA_FOURCC_I420) ? libyuv::FOURCC_I420 : libyuv::FOURCC_YUY2;

  if (libyuv_fourcc == libyuv::FOURCC_I420) {
    const auto* decoded_data_y =
        static_cast<const uint8_t*>(decoded_image->va_buffer()->data()) +
        decoded_image->image()->offsets[0];
    const auto* decoded_data_u =
        static_cast<const uint8_t*>(decoded_image->va_buffer()->data()) +
        decoded_image->image()->offsets[1];
    const auto* decoded_data_v =
        static_cast<const uint8_t*>(decoded_image->va_buffer()->data()) +
        decoded_image->image()->offsets[2];

    const double ssim = libyuv::I420Ssim(
        ref_y.get(), width, ref_u.get(), even_width, ref_v.get(), even_width,
        decoded_data_y,
        base::checked_cast<int>(decoded_image->image()->pitches[0]),
        decoded_data_u,
        base::checked_cast<int>(decoded_image->image()->pitches[1]),
        decoded_data_v,
        base::checked_cast<int>(decoded_image->image()->pitches[2]), width,
        height);
    if (ssim < kMinSsim) {
      DLOG(ERROR) << "Too low SSIM: " << ssim << " < " << kMinSsim;
      return false;
    }
  } else {
    auto temp_y = std::make_unique<uint8_t[]>(width * height);
    auto temp_u = std::make_unique<uint8_t[]>(even_width * even_height);
    auto temp_v = std::make_unique<uint8_t[]>(even_width * even_height);

    // TODO(crbug.com/868400): support other formats/planarities/pitches.
    constexpr uint32_t kNumPlanesYuv422 = 1u;
    constexpr uint32_t kBytesPerPixelYuv422 = 2u;
    if (decoded_image->image()->num_planes != kNumPlanesYuv422 ||
        decoded_image->image()->pitches[0] != (width * kBytesPerPixelYuv422)) {
      DLOG(ERROR) << "Too many planes (got "
                  << decoded_image->image()->num_planes << ", expected "
                  << kNumPlanesYuv422 << ") or rows not tightly packed (got "
                  << decoded_image->image()->pitches[0] << ", expected "
                  << (width * kBytesPerPixelYuv422) << "), aborting test";
      return false;
    }

    const int conversion_result = libyuv::ConvertToI420(
        static_cast<const uint8_t*>(decoded_image->va_buffer()->data()),
        base::strict_cast<size_t>(decoded_image->image()->data_size),
        temp_y.get(), width, temp_u.get(), even_width, temp_v.get(), even_width,
        0, 0, width, height, width, height, libyuv::kRotate0, libyuv_fourcc);
    if (conversion_result != 0) {
      DLOG(ERROR) << "libyuv conversion error";
      return false;
    }

    const double ssim = libyuv::I420Ssim(
        ref_y.get(), width, ref_u.get(), even_width, ref_v.get(), even_width,
        temp_y.get(), width, temp_u.get(), even_width, temp_v.get(), even_width,
        width, height);
    if (ssim < kMinSsim) {
      DLOG(ERROR) << "Too low SSIM: " << ssim << " < " << kMinSsim;
      return false;
    }
  }
  return true;
}

// Generates a checkerboard pattern as a JPEG image of a specified |size| and
// |subsampling| format. Returns an empty vector on failure.
std::vector<unsigned char> GenerateJpegImage(
    const gfx::Size& size,
    SkJpegEncoder::Downsample subsampling = SkJpegEncoder::Downsample::k420) {
  DCHECK(!size.IsEmpty());

  // First build a raw RGBA image of the given size with a checkerboard pattern.
  const SkImageInfo image_info = SkImageInfo::Make(
      size.width(), size.height(), SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kOpaque_SkAlphaType);
  const size_t byte_size = image_info.computeMinByteSize();
  if (byte_size == SIZE_MAX)
    return {};
  const size_t stride = image_info.minRowBytes();
  DCHECK_EQ(4, SkColorTypeBytesPerPixel(image_info.colorType()));
  DCHECK_EQ(4 * size.width(), base::checked_cast<int>(stride));
  constexpr gfx::Size kCheckerRectSize(3, 5);
  std::vector<uint8_t> rgba_data(byte_size);
  uint8_t* data = rgba_data.data();
  for (int y = 0; y < size.height(); y++) {
    const bool y_bit = (((y / kCheckerRectSize.height()) & 0x1) == 0);
    for (int x = 0; x < base::checked_cast<int>(stride); x += 4) {
      const bool x_bit = (((x / kCheckerRectSize.width()) & 0x1) == 0);
      const SkColor color = (x_bit != y_bit) ? SK_ColorBLUE : SK_ColorMAGENTA;
      data[x + 0] = SkColorGetR(color);
      data[x + 1] = SkColorGetG(color);
      data[x + 2] = SkColorGetB(color);
      data[x + 3] = SkColorGetA(color);
    }
    data += stride;
  }

  // Now, encode it as a JPEG.
  //
  // TODO(andrescj): if this generates a large enough image (in terms of byte
  // size), it will be decoded incorrectly in AMD Stoney Ridge (see
  // b/127874877). When that's resolved, change the quality here to 100 so that
  // the generated JPEG is large.
  std::vector<unsigned char> jpeg_data;
  if (gfx::JPEGCodec::Encode(
          SkPixmap(image_info, rgba_data.data(), stride) /* input */,
          95 /* quality */, subsampling /* downsample */,
          &jpeg_data /* output */)) {
    return jpeg_data;
  }
  return {};
}

// Rounds |n| to the greatest multiple of |m| that is less than or equal to |n|.
int RoundDownToMultiple(int n, int m) {
  DCHECK_GE(n, 0);
  DCHECK_GT(m, 0);
  return (n / m) * m;
}

// Rounds |n| to the smallest multiple of |m| that is greater than or equal to
// |n|.
int RoundUpToMultiple(int n, int m) {
  DCHECK_GE(n, 0);
  DCHECK_GT(m, 0);
  if (n % m == 0)
    return n;
  base::CheckedNumeric<int> safe_n(n);
  safe_n += m;
  return RoundDownToMultiple(safe_n.ValueOrDie(), m);
}

// Given a minimum supported surface dimension (width or height) value
// |min_surface_supported|, this function returns a non-zero coded dimension of
// a 4:2:0 JPEG image that would not be supported because the dimension is right
// below the supported value. For example, if |min_surface_supported| is 19,
// this function should return 16 because for a 4:2:0 image, both coded
// dimensions should be multiples of 16. If an unsupported dimension was found
// (i.e., |min_surface_supported| > 16), this function returns true, false
// otherwise.
bool GetMinUnsupportedDimension(int min_surface_supported,
                                int* min_unsupported) {
  if (min_surface_supported <= k420MCUSize)
    return false;
  *min_unsupported =
      RoundDownToMultiple(min_surface_supported - 1, k420MCUSize);
  return true;
}

// Given a minimum supported surface dimension (width or height) value
// |min_surface_supported|, this function returns a non-zero coded dimension of
// a 4:2:0 JPEG image that would be supported because the dimension is at least
// the minimum. For example, if |min_surface_supported| is 35, this function
// should return 48 because for a 4:2:0 image, both coded dimensions should be
// multiples of 16.
int GetMinSupportedDimension(int min_surface_supported) {
  if (min_surface_supported == 0)
    return k420MCUSize;
  return RoundUpToMultiple(min_surface_supported, k420MCUSize);
}

// Given a maximum supported surface dimension (width or height) value
// |max_surface_supported|, this function returns the coded dimension of a 4:2:0
// JPEG image that would be supported because the dimension is at most the
// maximum. For example, if |max_surface_supported| is 65, this function
// should return 64 because for a 4:2:0 image, both coded dimensions should be
// multiples of 16.
int GetMaxSupportedDimension(int max_surface_supported) {
  return RoundDownToMultiple(max_surface_supported, k420MCUSize);
}

}  // namespace

class VaapiJpegDecoderTest : public testing::TestWithParam<TestParam> {
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

  std::unique_ptr<ScopedVAImage> Decode(
      base::span<const uint8_t> encoded_image,
      VaapiJpegDecodeStatus* status = nullptr);

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

std::unique_ptr<ScopedVAImage> VaapiJpegDecoderTest::Decode(
    base::span<const uint8_t> encoded_image,
    VaapiJpegDecodeStatus* status) {
  VaapiJpegDecodeStatus tmp_status;
  std::unique_ptr<ScopedVAImage> scoped_image =
      decoder_.DoDecode(encoded_image, &tmp_status);
  EXPECT_EQ(!!scoped_image, tmp_status == VaapiJpegDecodeStatus::kSuccess);
  if (status)
    *status = tmp_status;
  return scoped_image;
}

TEST_P(VaapiJpegDecoderTest, DecodeSucceeds) {
  base::FilePath input_file = FindTestDataFilePath(GetParam().filename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();

  const auto encoded_image = base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size());
  std::unique_ptr<ScopedVAImage> scoped_image = Decode(encoded_image);
  ASSERT_TRUE(scoped_image);

  ASSERT_TRUE(CompareImages(encoded_image, scoped_image.get()));
}

// Make sure that JPEGs whose size is in the supported size range are decoded
// successfully.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
TEST_F(VaapiJpegDecoderTest, DecodeSucceedsForSupportedSizes) {
  gfx::Size min_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetJpegDecodeMinResolution(&min_supported_size));
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetJpegDecodeMaxResolution(&max_supported_size));

  // Ensure the maximum supported size is reasonable.
  ASSERT_GE(max_supported_size.width(), min_supported_size.width());
  ASSERT_GE(max_supported_size.height(), min_supported_size.height());
  ASSERT_LE(max_supported_size.width(), kLargestSupportedSize.width());
  ASSERT_LE(max_supported_size.height(), kLargestSupportedSize.height());

  // The actual image min/max coded size depends on the subsampling format. For
  // example, for 4:2:0, the coded dimensions must be multiples of 16. So, if
  // the minimum surface size is, e.g., 18x18, the minimum image coded size is
  // 32x32. Get those actual min/max coded sizes now.
  const int min_width = GetMinSupportedDimension(min_supported_size.width());
  const int min_height = GetMinSupportedDimension(min_supported_size.height());
  const int max_width = GetMaxSupportedDimension(max_supported_size.width());
  const int max_height = GetMaxSupportedDimension(max_supported_size.height());
  ASSERT_GT(max_width, 0);
  ASSERT_GT(max_height, 0);
  const std::vector<gfx::Size> test_sizes = {{min_width, min_height},
                                             {min_width, max_height},
                                             {max_width, min_height},
                                             {max_width, max_height}};
  for (const auto& test_size : test_sizes) {
    const std::vector<unsigned char> jpeg_data = GenerateJpegImage(test_size);
    auto jpeg_data_span = base::make_span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size());
    ASSERT_FALSE(jpeg_data.empty());
    std::unique_ptr<ScopedVAImage> scoped_image = Decode(jpeg_data_span);
    ASSERT_TRUE(scoped_image)
        << "Decode unexpectedly failed for size = " << test_size.ToString();
    EXPECT_TRUE(CompareImages(jpeg_data_span, scoped_image.get()))
        << "The SSIM check unexpectedly failed for size = "
        << test_size.ToString();
  }
}

// Make sure that JPEGs whose size is below the supported size range are
// rejected.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
TEST_F(VaapiJpegDecoderTest, DecodeFailsForBelowMinSize) {
  gfx::Size min_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetJpegDecodeMinResolution(&min_supported_size));
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetJpegDecodeMaxResolution(&max_supported_size));

  // Ensure the maximum supported size is reasonable.
  ASSERT_GE(max_supported_size.width(), min_supported_size.width());
  ASSERT_GE(max_supported_size.height(), min_supported_size.height());
  ASSERT_LE(max_supported_size.width(), kLargestSupportedSize.width());
  ASSERT_LE(max_supported_size.height(), kLargestSupportedSize.height());

  // Get good (supported) minimum dimensions.
  const int good_width = GetMinSupportedDimension(min_supported_size.width());
  ASSERT_LE(good_width, max_supported_size.width());
  const int good_height = GetMinSupportedDimension(min_supported_size.height());
  ASSERT_LE(good_height, max_supported_size.height());

  // Get bad (unsupported) dimensions.
  int bad_width;
  const bool got_bad_width =
      GetMinUnsupportedDimension(min_supported_size.width(), &bad_width);
  int bad_height;
  const bool got_bad_height =
      GetMinUnsupportedDimension(min_supported_size.height(), &bad_height);

  // Now build and test the good/bad combinations that we expect will fail.
  std::vector<gfx::Size> test_sizes;
  if (got_bad_width)
    test_sizes.push_back({bad_width, good_height});
  if (got_bad_height)
    test_sizes.push_back({good_width, bad_height});
  if (got_bad_width && got_bad_height)
    test_sizes.push_back({bad_width, bad_height});
  for (const auto& test_size : test_sizes) {
    const std::vector<unsigned char> jpeg_data = GenerateJpegImage(test_size);
    ASSERT_FALSE(jpeg_data.empty());
    VaapiJpegDecodeStatus status = VaapiJpegDecodeStatus::kSuccess;
    ASSERT_FALSE(Decode(base::make_span<const uint8_t>(
                            reinterpret_cast<const uint8_t*>(jpeg_data.data()),
                            jpeg_data.size()),
                        &status))
        << "Decode unexpectedly succeeded for size = " << test_size.ToString();
    EXPECT_EQ(VaapiJpegDecodeStatus::kUnsupportedJpeg, status);
  }
}

// Make sure that JPEGs whose size is above the supported size range are
// rejected.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
TEST_F(VaapiJpegDecoderTest, DecodeFailsForAboveMaxSize) {
  gfx::Size min_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetJpegDecodeMinResolution(&min_supported_size));
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetJpegDecodeMaxResolution(&max_supported_size));

  // Ensure the maximum supported size is reasonable.
  ASSERT_GE(max_supported_size.width(), min_supported_size.width());
  ASSERT_GE(max_supported_size.height(), min_supported_size.height());
  ASSERT_LE(max_supported_size.width(), kLargestSupportedSize.width());
  ASSERT_LE(max_supported_size.height(), kLargestSupportedSize.height());

  // Get good (supported) maximum dimensions.
  const int good_width = GetMaxSupportedDimension(max_supported_size.width());
  ASSERT_GE(good_width, min_supported_size.width());
  ASSERT_GT(good_width, 0);
  const int good_height = GetMaxSupportedDimension(max_supported_size.height());
  ASSERT_GE(good_height, min_supported_size.height());
  ASSERT_GT(good_height, 0);

  // Get bad (unsupported) dimensions.
  const int bad_width =
      RoundUpToMultiple(max_supported_size.width() + 1, k420MCUSize);
  const int bad_height =
      RoundUpToMultiple(max_supported_size.height() + 1, k420MCUSize);

  // Now build and test the good/bad combinations that we expect will fail.
  const std::vector<gfx::Size> test_sizes = {{bad_width, good_height},
                                             {good_width, bad_height},
                                             {bad_width, bad_height}};
  for (const auto& test_size : test_sizes) {
    const std::vector<unsigned char> jpeg_data = GenerateJpegImage(test_size);
    ASSERT_FALSE(jpeg_data.empty());
    VaapiJpegDecodeStatus status = VaapiJpegDecodeStatus::kSuccess;
    ASSERT_FALSE(Decode(base::make_span<const uint8_t>(
                            reinterpret_cast<const uint8_t*>(jpeg_data.data()),
                            jpeg_data.size()),
                        &status))
        << "Decode unexpectedly succeeded for size = " << test_size.ToString();
    EXPECT_EQ(VaapiJpegDecodeStatus::kUnsupportedJpeg, status);
  }
}

TEST_F(VaapiJpegDecoderTest, DecodeFails) {
  // Not supported by VAAPI.
  base::FilePath input_file = FindTestDataFilePath(kUnsupportedFilename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  VaapiJpegDecodeStatus status = VaapiJpegDecodeStatus::kSuccess;
  ASSERT_FALSE(Decode(
      base::make_span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size()),
      &status));
  EXPECT_EQ(VaapiJpegDecodeStatus::kUnsupportedJpeg, status);
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

INSTANTIATE_TEST_SUITE_P(, VaapiJpegDecoderTest, testing::ValuesIn(kTestCases));

}  // namespace media
