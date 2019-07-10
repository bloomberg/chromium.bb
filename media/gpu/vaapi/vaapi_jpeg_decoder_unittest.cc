// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
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
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_types.h"
#include "media/capture/video/chromeos/local_gpu_memory_buffer_manager.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/jpeg_parser.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {
namespace {

constexpr const char* kYuv422Filename = "pixel-1280x720.jpg";
constexpr const char* kYuv420Filename = "pixel-1280x720-yuv420.jpg";
constexpr const char* kYuv444Filename = "pixel-1280x720-yuv444.jpg";
constexpr const char* kOddHeightImageFilename = "peach_pi-40x23.jpg";
constexpr const char* kOddWidthImageFilename = "peach_pi-41x22.jpg";
constexpr const char* kOddDimensionsImageFilename = "peach_pi-41x23.jpg";

struct TestParam {
  const char* test_name;
  const char* filename;
};

const TestParam kTestCases[] = {
    {"YUV422", kYuv422Filename},
    {"YUV420", kYuv420Filename},
    {"YUV444", kYuv444Filename},
    {"OddHeightImage40x23", kOddHeightImageFilename},
    {"OddWidthImage41x22", kOddWidthImageFilename},
    {"OddDimensionsImage41x23", kOddDimensionsImageFilename},
};

constexpr size_t kMaxNumberPlanes = base::size(VAImage().pitches);
static_assert(kMaxNumberPlanes <= 3u, "The number of planes should be <= 3");
static_assert(
    base::size(VAImage().pitches) == base::size(VAImage().offsets),
    "The number of VAImage pitches is not equal to the number of offsets");

struct DecodedVAImage {
  uint32_t va_fourcc;
  uint32_t number_of_planes;  // Can not be greater than kMaxNumberPlanes.
  gfx::Size coded_size;
  struct {
    uint8_t* data;
    int stride;
  } planes[kMaxNumberPlanes];
};

constexpr double kMinSsim = 0.995;

// This file is not supported by the VAAPI, so we don't define expectations on
// the decode result.
constexpr const char* kUnsupportedFilename = "pixel-1280x720-grayscale.jpg";

// The size of the minimum coded unit for a YUV 4:2:0 image (both the width and
// the height of the MCU are the same for 4:2:0).
constexpr int k420MCUSize = 16;

// The largest maximum supported surface size we expect a driver to report for
// JPEG decoding.
constexpr gfx::Size kLargestSupportedSize(16 * 1024, 16 * 1024);

// Takes a ScopedVAImage and returns a DecodedVAImage object that represents
// the same decoded result.
DecodedVAImage ScopedVAImageToDecodedVAImage(
    const ScopedVAImage* decoded_image) {
  DecodedVAImage converted_result{};

  converted_result.va_fourcc = decoded_image->image()->format.fourcc;
  converted_result.number_of_planes = decoded_image->image()->num_planes;
  converted_result.coded_size =
      gfx::Size(base::strict_cast<int>(decoded_image->image()->width),
                base::strict_cast<int>(decoded_image->image()->height));

  DCHECK_LE(base::strict_cast<size_t>(converted_result.number_of_planes),
            kMaxNumberPlanes);

  // This is safe because |number_of_planes| is retrieved from the VA-API and it
  // can not be greater than 3, which is also the size of the |planes| array.
  for (uint32_t i = 0u; i < converted_result.number_of_planes; ++i) {
    converted_result.planes[i].data =
        static_cast<uint8_t*>(decoded_image->va_buffer()->data()) +
        decoded_image->image()->offsets[i];
    converted_result.planes[i].stride =
        base::checked_cast<int>(decoded_image->image()->pitches[i]);
  }

  return converted_result;
}

// Compares the result of sw decoding |encoded_image| with |decoded_image| using
// SSIM. Returns true if all conversions work and SSIM is above a given
// threshold (kMinSsim), or false otherwise.
bool CompareImages(base::span<const uint8_t> encoded_image,
                   const DecodedVAImage& decoded_image) {
  JpegParseResult parse_result;
  const bool result = ParseJpegPicture(encoded_image.data(),
                                       encoded_image.size(), &parse_result);
  if (!result)
    return false;

  const int coded_width =
      base::strict_cast<int>(parse_result.frame_header.coded_width);
  const int coded_height =
      base::strict_cast<int>(parse_result.frame_header.coded_height);
  // Note the use of > instead of !=. This is to handle the case in the Intel
  // iHD driver where the coded size we compute for one of the images is
  // 1280x720 while the size of the VAAPI surface is 1280x736 because of
  // additional alignment. See https://git.io/fj6nA.
  if (coded_width > decoded_image.coded_size.width() ||
      coded_height > decoded_image.coded_size.height()) {
    DLOG(ERROR) << "Wrong expected decoded JPEG coded size, " << coded_width
                << "x" << coded_height << " versus VaAPI provided "
                << decoded_image.coded_size.width() << "x"
                << decoded_image.coded_size.height();
    return false;
  }

  const uint16_t width = parse_result.frame_header.visible_width;
  const uint16_t height = parse_result.frame_header.visible_height;
  const uint16_t half_width = (width + 1) / 2;
  const uint16_t half_height = (height + 1) / 2;

  auto libyuv_y_plane = std::make_unique<uint8_t[]>(width * height);
  auto libyuv_u_plane = std::make_unique<uint8_t[]>(half_width * half_height);
  auto libyuv_v_plane = std::make_unique<uint8_t[]>(half_width * half_height);

  int conversion_result = libyuv::ConvertToI420(
      encoded_image.data(), encoded_image.size(), libyuv_y_plane.get(), width,
      libyuv_u_plane.get(), half_width, libyuv_v_plane.get(), half_width, 0, 0,
      width, height, width, height, libyuv::kRotate0, libyuv::FOURCC_MJPG);
  if (conversion_result != 0) {
    DLOG(ERROR) << "libyuv conversion error";
    return false;
  }

  const uint32_t va_fourcc = decoded_image.va_fourcc;
  double ssim = 0;
  if (va_fourcc == VA_FOURCC_I420) {
    ssim = libyuv::I420Ssim(
        libyuv_y_plane.get(), width, libyuv_u_plane.get(), half_width,
        libyuv_v_plane.get(), half_width, decoded_image.planes[0].data,
        decoded_image.planes[0].stride, decoded_image.planes[1].data,
        decoded_image.planes[1].stride, decoded_image.planes[2].data,
        decoded_image.planes[2].stride, width, height);
  } else if (va_fourcc == VA_FOURCC_NV12 || va_fourcc == VA_FOURCC_YUY2 ||
             va_fourcc == VA_FOURCC('Y', 'U', 'Y', 'V')) {
    // Temporary planes to hold intermediate conversions to I420 (i.e. NV12 to
    // I420 or YUYV/2 to I420).
    auto temp_y = std::make_unique<uint8_t[]>(width * height);
    auto temp_u = std::make_unique<uint8_t[]>(half_width * half_height);
    auto temp_v = std::make_unique<uint8_t[]>(half_width * half_height);

    if (va_fourcc == VA_FOURCC_NV12) {
      conversion_result = libyuv::NV12ToI420(
          decoded_image.planes[0].data, decoded_image.planes[0].stride,
          decoded_image.planes[1].data, decoded_image.planes[1].stride,
          temp_y.get(), width, temp_u.get(), half_width, temp_v.get(),
          half_width, width, height);
    } else {
      // |va_fourcc| is YUY2 or YUYV, which are handled the same.
      // TODO(crbug.com/868400): support other formats/planarities/pitches.
      conversion_result = libyuv::YUY2ToI420(
          decoded_image.planes[0].data, decoded_image.planes[0].stride,
          temp_y.get(), width, temp_u.get(), half_width, temp_v.get(),
          half_width, width, height);
    }
    if (conversion_result != 0) {
      DLOG(ERROR) << "libyuv conversion error";
      return false;
    }

    ssim = libyuv::I420Ssim(libyuv_y_plane.get(), width, libyuv_u_plane.get(),
                            half_width, libyuv_v_plane.get(), half_width,
                            temp_y.get(), width, temp_u.get(), half_width,
                            temp_v.get(), half_width, width, height);
  } else {
    DLOG(ERROR) << "FourCC not supported: " << FourccToString(va_fourcc);
    return false;
  }

  if (ssim < kMinSsim) {
    DLOG(ERROR) << "SSIM too low: " << ssim << " < " << kMinSsim;
    return false;
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
  LOG_ASSERT(min_surface_supported > 0);
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

class VASurface;

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
      uint32_t preferred_fourcc,
      VaapiImageDecodeStatus* status = nullptr);

  std::unique_ptr<ScopedVAImage> Decode(
      base::span<const uint8_t> encoded_image,
      VaapiImageDecodeStatus* status = nullptr);

  scoped_refptr<gfx::NativePixmapDmaBuf> DecodeToNativePixmapDmaBuf(
      base::span<const uint8_t> encoded_image,
      VaapiImageDecodeStatus* status = nullptr);

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
    uint32_t preferred_fourcc,
    VaapiImageDecodeStatus* status) {
  VaapiImageDecodeStatus decode_status;
  scoped_refptr<VASurface> surface =
      decoder_.Decode(encoded_image, &decode_status);
  EXPECT_EQ(!!surface, decode_status == VaapiImageDecodeStatus::kSuccess);

  // Still try to get image when decode fails.
  VaapiImageDecodeStatus image_status;
  std::unique_ptr<ScopedVAImage> scoped_image;
  scoped_image = decoder_.GetImage(preferred_fourcc, &image_status);
  EXPECT_EQ(!!scoped_image, image_status == VaapiImageDecodeStatus::kSuccess);

  // Return the first fail status.
  if (status) {
    *status = decode_status != VaapiImageDecodeStatus::kSuccess ? decode_status
                                                                : image_status;
  }
  return scoped_image;
}

std::unique_ptr<ScopedVAImage> VaapiJpegDecoderTest::Decode(
    base::span<const uint8_t> encoded_image,
    VaapiImageDecodeStatus* status) {
  return Decode(encoded_image, VA_FOURCC_I420, status);
}

scoped_refptr<gfx::NativePixmapDmaBuf>
VaapiJpegDecoderTest::DecodeToNativePixmapDmaBuf(
    base::span<const uint8_t> encoded_image,
    VaapiImageDecodeStatus* status) {
  VaapiImageDecodeStatus decode_status;
  scoped_refptr<VASurface> surface =
      decoder_.Decode(encoded_image, &decode_status);
  EXPECT_EQ(!!surface, decode_status == VaapiImageDecodeStatus::kSuccess);

  // Still try to get the pixmap when decode fails.
  VaapiImageDecodeStatus pixmap_status;
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap =
      decoder_.ExportAsNativePixmapDmaBuf(&pixmap_status);
  EXPECT_EQ(!!pixmap, pixmap_status == VaapiImageDecodeStatus::kSuccess);

  // Return the first fail status.
  if (status) {
    *status = decode_status != VaapiImageDecodeStatus::kSuccess ? decode_status
                                                                : pixmap_status;
  }
  return pixmap;
}

// The intention of this test is to ensure that the workarounds added in
// VaapiWrapper::GetJpegDecodeSuitableImageFourCC() don't result in an
// unsupported image format.
TEST_F(VaapiJpegDecoderTest, MinimalImageFormatSupport) {
  // All drivers should support at least I420.
  VAImageFormat i420_format{};
  i420_format.fourcc = VA_FOURCC_I420;
  ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(i420_format));

  // Additionally, the mesa VAAPI driver should support YV12, NV12 and YUYV.
  if (base::StartsWith(VaapiWrapper::GetVendorStringForTesting(),
                       "Mesa Gallium driver", base::CompareCase::SENSITIVE)) {
    VAImageFormat yv12_format{};
    yv12_format.fourcc = VA_FOURCC_YV12;
    ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(yv12_format));

    VAImageFormat nv12_format{};
    nv12_format.fourcc = VA_FOURCC_NV12;
    ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(nv12_format));

    VAImageFormat yuyv_format{};
    yuyv_format.fourcc = VA_FOURCC('Y', 'U', 'Y', 'V');
    ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(yuyv_format));
  }
}

TEST_F(VaapiJpegDecoderTest, ReleaseVASurface) {
  base::FilePath input_file = FindTestDataFilePath(kYuv420Filename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  const auto encoded_image = base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size());

  VAImageFormat i420_format{};
  i420_format.fourcc = VA_FOURCC_I420;
  EXPECT_TRUE(VaapiWrapper::IsImageFormatSupported(i420_format));

  // Get the first VASurface and validate success.
  VaapiImageDecodeStatus decode_status;
  scoped_refptr<VASurface> pre_release_surface =
      decoder_.Decode(encoded_image, &decode_status);
  ASSERT_TRUE(pre_release_surface);
  ASSERT_EQ(VaapiImageDecodeStatus::kSuccess, decode_status);

  // Take ownership of the |decoder_| surface.
  scoped_refptr<VASurface> post_release_surface = decoder_.ReleaseVASurface();
  ASSERT_TRUE(post_release_surface);

  EXPECT_EQ(pre_release_surface->id(), post_release_surface->id());
  EXPECT_EQ(pre_release_surface->size(), post_release_surface->size());
  EXPECT_EQ(pre_release_surface->format(), post_release_surface->format());

  // GetImage() should fail now because ReleaseVASurface() removed the internal
  // |decoder_| surface.
  VaapiImageDecodeStatus image_status;
  ASSERT_FALSE(decoder_.GetImage(i420_format.fourcc, &image_status));
  ASSERT_EQ(VaapiImageDecodeStatus::kInvalidState, image_status);

  // Decode the same image again and get the new VASurface.
  scoped_refptr<VASurface> new_surface =
      decoder_.Decode(encoded_image, &decode_status);
  ASSERT_TRUE(new_surface);
  ASSERT_EQ(VaapiImageDecodeStatus::kSuccess, decode_status);

  // The new VASurface should have a different surface ID than that of the old
  // one, but same size and format since the same exact image was decoded.
  EXPECT_NE(new_surface->id(), post_release_surface->id());
  EXPECT_EQ(new_surface->size(), post_release_surface->size());
  EXPECT_EQ(new_surface->format(), post_release_surface->format());
}

TEST_P(VaapiJpegDecoderTest, DecodeSucceeds) {
  base::FilePath input_file = FindTestDataFilePath(GetParam().filename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  const auto encoded_image = base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size());

  // Skip the image if the VAAPI driver doesn't claim to support its chroma
  // subsampling format. However, we expect at least 4:2:0 and 4:2:2 support.
  const VaapiWrapper::InternalFormats supported_internal_formats =
      VaapiWrapper::GetDecodeSupportedInternalFormats(VAProfileJPEGBaseline);
  ASSERT_TRUE(supported_internal_formats.yuv420);
  ASSERT_TRUE(supported_internal_formats.yuv422);
  JpegParseResult parse_result;
  ASSERT_TRUE(ParseJpegPicture(encoded_image.data(), encoded_image.size(),
                               &parse_result));
  const unsigned int rt_format =
      VaSurfaceFormatForJpeg(parse_result.frame_header);
  ASSERT_NE(kInvalidVaRtFormat, rt_format);
  if (!VaapiWrapper::IsDecodingSupportedForInternalFormat(VAProfileJPEGBaseline,
                                                          rt_format)) {
    GTEST_SKIP();
  }

  // Note that this test together with
  // VaapiJpegDecoderTest.MinimalImageFormatSupport gives us two guarantees:
  //
  // 1) Every combination of supported internal format (returned by
  //    GetJpegDecodeSupportedInternalFormats()) and supported image format
  //    works with vaGetImage() (for JPEG decoding).
  //
  // 2) The FOURCC returned by VaapiWrapper::GetJpegDecodeSuitableImageFourCC()
  //    corresponds to a supported image format.
  //
  // Note that we expect VA_FOURCC_I420 and VA_FOURCC_NV12 support in all
  // drivers.
  const std::vector<VAImageFormat>& supported_image_formats =
      VaapiWrapper::GetSupportedImageFormatsForTesting();
  EXPECT_GE(supported_image_formats.size(), 2u);

  VAImageFormat i420_format{};
  i420_format.fourcc = VA_FOURCC_I420;
  EXPECT_TRUE(VaapiWrapper::IsImageFormatSupported(i420_format));

  VAImageFormat nv12_format{};
  nv12_format.fourcc = VA_FOURCC_NV12;
  EXPECT_TRUE(VaapiWrapper::IsImageFormatSupported(nv12_format));

  for (const auto& image_format : supported_image_formats) {
    std::unique_ptr<ScopedVAImage> scoped_image =
        Decode(encoded_image, image_format.fourcc);
    ASSERT_TRUE(scoped_image);
    const uint32_t actual_fourcc = scoped_image->image()->format.fourcc;
    // TODO(andrescj): CompareImages() only supports I420, NV12, YUY2, and YUYV.
    // Make it support all the image formats we expect and call it
    // unconditionally.
    if (actual_fourcc == VA_FOURCC_I420 || actual_fourcc == VA_FOURCC_NV12 ||
        actual_fourcc == VA_FOURCC_YUY2 ||
        actual_fourcc == VA_FOURCC('Y', 'U', 'Y', 'V')) {
      ASSERT_TRUE(CompareImages(
          encoded_image, ScopedVAImageToDecodedVAImage(scoped_image.get())));
    }
    DVLOG(1) << "Got a " << FourccToString(scoped_image->image()->format.fourcc)
             << " VAImage (preferred " << FourccToString(image_format.fourcc)
             << ")";
  }
}

// Make sure that JPEGs whose size is in the supported size range are decoded
// successfully.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
TEST_F(VaapiJpegDecoderTest, DecodeSucceedsForSupportedSizes) {
  gfx::Size min_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetDecodeMinResolution(VAProfileJPEGBaseline,
                                                   &min_supported_size));
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetDecodeMaxResolution(VAProfileJPEGBaseline,
                                                   &max_supported_size));

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
    EXPECT_TRUE(CompareImages(
        jpeg_data_span, ScopedVAImageToDecodedVAImage(scoped_image.get())))
        << "The SSIM check unexpectedly failed for size = "
        << test_size.ToString();
  }
}

// TODO(andrescj): test other JPEG formats besides YUV 4:2:0.
TEST_F(VaapiJpegDecoderTest, DecodeAndExportAsNativePixmapDmaBuf) {
  if (base::StartsWith(VaapiWrapper::GetVendorStringForTesting(),
                       "Mesa Gallium driver", base::CompareCase::SENSITIVE)) {
    // TODO(crbug.com/974438): until we support surfaces with multiple buffer
    // objects, the AMD driver fails this test.
    GTEST_SKIP();
  }
  if (base::StartsWith(VaapiWrapper::GetVendorStringForTesting(),
                       "Intel i965 driver", base::CompareCase::SENSITIVE)) {
    // TODO(b/135705575): until the correct offsets are exported, the Intel i965
    // driver fails this test.
    GTEST_SKIP();
  }

  base::FilePath input_file = FindTestDataFilePath(kYuv420Filename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  const auto encoded_image = base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size());
  VaapiImageDecodeStatus status;
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap =
      DecodeToNativePixmapDmaBuf(encoded_image, &status);
  ASSERT_EQ(VaapiImageDecodeStatus::kSuccess, status);
  ASSERT_TRUE(pixmap);

  // After exporting the surface, we should not be able to obtain a VAImage with
  // the decoded data.
  VAImageFormat i420_format{};
  i420_format.fourcc = VA_FOURCC_I420;
  EXPECT_TRUE(VaapiWrapper::IsImageFormatSupported(i420_format));
  EXPECT_FALSE(decoder_.GetImage(i420_format.fourcc, &status));
  EXPECT_EQ(VaapiImageDecodeStatus::kInvalidState, status);

  // Workaround: in order to import and map the pixmap using minigbm when the
  // format is gfx::BufferFormat::YVU_420, we need to reorder the planes so that
  // the offsets are in increasing order as assumed in https://bit.ly/2NLubNN.
  // Otherwise, we get a validation error. In essence, we're making minigbm
  // think that it is mapping a YVU_420, but it's actually mapping a YUV_420.
  //
  // TODO(andrescj): revisit this once crrev.com/c/1573718 lands.
  gfx::NativePixmapHandle handle = pixmap->ExportHandle();
  if (pixmap->GetBufferFormat() == gfx::BufferFormat::YVU_420)
    std::swap(handle.planes[1], handle.planes[2]);

  LocalGpuMemoryBufferManager gpu_memory_buffer_manager;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      gpu_memory_buffer_manager.ImportDmaBuf(handle, pixmap->GetBufferSize(),
                                             pixmap->GetBufferFormat());
  ASSERT_TRUE(gpu_memory_buffer);
  ASSERT_TRUE(gpu_memory_buffer->Map());
  DecodedVAImage decoded_image{};
  const gfx::BufferFormat format = gpu_memory_buffer->GetFormat();
  if (format == gfx::BufferFormat::YVU_420) {
    decoded_image.va_fourcc = VA_FOURCC_I420;
    decoded_image.number_of_planes = 3u;
  } else if (format == gfx::BufferFormat::YUV_420_BIPLANAR) {
    decoded_image.va_fourcc = VA_FOURCC_NV12;
    decoded_image.number_of_planes = 2u;
  } else {
    ASSERT_TRUE(false) << "Unsupported format "
                       << gfx::BufferFormatToString(format);
  }
  decoded_image.coded_size = gpu_memory_buffer->GetSize();
  for (size_t plane = 0u;
       plane < base::strict_cast<size_t>(decoded_image.number_of_planes);
       plane++) {
    decoded_image.planes[plane].data =
        static_cast<uint8_t*>(gpu_memory_buffer->memory(plane));
    decoded_image.planes[plane].stride = gpu_memory_buffer->stride(plane);
  }
  EXPECT_TRUE(CompareImages(encoded_image, decoded_image));
  gpu_memory_buffer->Unmap();
}

// Make sure that JPEGs whose size is below the supported size range are
// rejected.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
TEST_F(VaapiJpegDecoderTest, DecodeFailsForBelowMinSize) {
  gfx::Size min_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetDecodeMinResolution(VAProfileJPEGBaseline,
                                                   &min_supported_size));
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetDecodeMaxResolution(VAProfileJPEGBaseline,
                                                   &max_supported_size));

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
    VaapiImageDecodeStatus status = VaapiImageDecodeStatus::kSuccess;
    ASSERT_FALSE(Decode(base::make_span<const uint8_t>(
                            reinterpret_cast<const uint8_t*>(jpeg_data.data()),
                            jpeg_data.size()),
                        &status))
        << "Decode unexpectedly succeeded for size = " << test_size.ToString();
    EXPECT_EQ(VaapiImageDecodeStatus::kUnsupportedImage, status);
  }
}

// Make sure that JPEGs whose size is above the supported size range are
// rejected.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
TEST_F(VaapiJpegDecoderTest, DecodeFailsForAboveMaxSize) {
  gfx::Size min_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetDecodeMinResolution(VAProfileJPEGBaseline,
                                                   &min_supported_size));
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetDecodeMaxResolution(VAProfileJPEGBaseline,
                                                   &max_supported_size));

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
    VaapiImageDecodeStatus status = VaapiImageDecodeStatus::kSuccess;
    ASSERT_FALSE(Decode(base::make_span<const uint8_t>(
                            reinterpret_cast<const uint8_t*>(jpeg_data.data()),
                            jpeg_data.size()),
                        &status))
        << "Decode unexpectedly succeeded for size = " << test_size.ToString();
    EXPECT_EQ(VaapiImageDecodeStatus::kUnsupportedImage, status);
  }
}

TEST_F(VaapiJpegDecoderTest, DecodeFails) {
  // A grayscale image (4:0:0) should be rejected.
  base::FilePath input_file = FindTestDataFilePath(kUnsupportedFilename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  VaapiImageDecodeStatus status = VaapiImageDecodeStatus::kSuccess;
  ASSERT_FALSE(Decode(
      base::make_span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size()),
      &status));
  EXPECT_EQ(VaapiImageDecodeStatus::kUnsupportedSubsampling, status);
}

std::string TestParamToString(
    const testing::TestParamInfo<TestParam>& param_info) {
  return param_info.param.test_name;
}

INSTANTIATE_TEST_SUITE_P(,
                         VaapiJpegDecoderTest,
                         testing::ValuesIn(kTestCases),
                         TestParamToString);

}  // namespace media
