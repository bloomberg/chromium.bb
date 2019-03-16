// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"

#include <string.h>

#include <iostream>
#include <type_traits>
#include <vector>

#include <va/va.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

namespace {

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

constexpr unsigned int kInvalidVaRtFormat = 0u;

static void FillPictureParameters(
    const JpegFrameHeader& frame_header,
    VAPictureParameterBufferJPEGBaseline* pic_param) {
  pic_param->picture_width = frame_header.coded_width;
  pic_param->picture_height = frame_header.coded_height;
  pic_param->num_components = frame_header.num_components;

  for (int i = 0; i < pic_param->num_components; i++) {
    pic_param->components[i].component_id = frame_header.components[i].id;
    pic_param->components[i].h_sampling_factor =
        frame_header.components[i].horizontal_sampling_factor;
    pic_param->components[i].v_sampling_factor =
        frame_header.components[i].vertical_sampling_factor;
    pic_param->components[i].quantiser_table_selector =
        frame_header.components[i].quantization_table_selector;
  }
}

static void FillIQMatrix(const JpegQuantizationTable* q_table,
                         VAIQMatrixBufferJPEGBaseline* iq_matrix) {
  static_assert(kJpegMaxQuantizationTableNum ==
                    std::extent<decltype(iq_matrix->load_quantiser_table)>(),
                "max number of quantization table mismatched");
  static_assert(
      sizeof(iq_matrix->quantiser_table[0]) == sizeof(q_table[0].value),
      "number of quantization entries mismatched");
  for (size_t i = 0; i < kJpegMaxQuantizationTableNum; i++) {
    if (!q_table[i].valid)
      continue;
    iq_matrix->load_quantiser_table[i] = 1;
    for (size_t j = 0; j < base::size(q_table[i].value); j++)
      iq_matrix->quantiser_table[i][j] = q_table[i].value[j];
  }
}

static void FillHuffmanTable(const JpegHuffmanTable* dc_table,
                             const JpegHuffmanTable* ac_table,
                             VAHuffmanTableBufferJPEGBaseline* huffman_table) {
  // Use default huffman tables if not specified in header.
  bool has_huffman_table = false;
  for (size_t i = 0; i < kJpegMaxHuffmanTableNumBaseline; i++) {
    if (dc_table[i].valid || ac_table[i].valid) {
      has_huffman_table = true;
      break;
    }
  }
  if (!has_huffman_table) {
    dc_table = kDefaultDcTable;
    ac_table = kDefaultAcTable;
  }

  static_assert(kJpegMaxHuffmanTableNumBaseline ==
                    std::extent<decltype(huffman_table->load_huffman_table)>(),
                "max number of huffman table mismatched");
  static_assert(sizeof(huffman_table->huffman_table[0].num_dc_codes) ==
                    sizeof(dc_table[0].code_length),
                "size of huffman table code length mismatch");
  static_assert(sizeof(huffman_table->huffman_table[0].dc_values[0]) ==
                    sizeof(dc_table[0].code_value[0]),
                "size of huffman table code value mismatch");
  for (size_t i = 0; i < kJpegMaxHuffmanTableNumBaseline; i++) {
    if (!dc_table[i].valid || !ac_table[i].valid)
      continue;
    huffman_table->load_huffman_table[i] = 1;

    memcpy(huffman_table->huffman_table[i].num_dc_codes,
           dc_table[i].code_length,
           sizeof(huffman_table->huffman_table[i].num_dc_codes));
    memcpy(huffman_table->huffman_table[i].dc_values, dc_table[i].code_value,
           sizeof(huffman_table->huffman_table[i].dc_values));
    memcpy(huffman_table->huffman_table[i].num_ac_codes,
           ac_table[i].code_length,
           sizeof(huffman_table->huffman_table[i].num_ac_codes));
    memcpy(huffman_table->huffman_table[i].ac_values, ac_table[i].code_value,
           sizeof(huffman_table->huffman_table[i].ac_values));
  }
}

static void FillSliceParameters(
    const JpegParseResult& parse_result,
    VASliceParameterBufferJPEGBaseline* slice_param) {
  slice_param->slice_data_size = parse_result.data_size;
  slice_param->slice_data_offset = 0;
  slice_param->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param->slice_horizontal_position = 0;
  slice_param->slice_vertical_position = 0;
  slice_param->num_components = parse_result.scan.num_components;
  for (int i = 0; i < slice_param->num_components; i++) {
    slice_param->components[i].component_selector =
        parse_result.scan.components[i].component_selector;
    slice_param->components[i].dc_table_selector =
        parse_result.scan.components[i].dc_selector;
    slice_param->components[i].ac_table_selector =
        parse_result.scan.components[i].ac_selector;
  }
  slice_param->restart_interval = parse_result.restart_interval;

  // Cast to int to prevent overflow.
  int max_h_factor =
      parse_result.frame_header.components[0].horizontal_sampling_factor;
  int max_v_factor =
      parse_result.frame_header.components[0].vertical_sampling_factor;
  int mcu_cols = parse_result.frame_header.coded_width / (max_h_factor * 8);
  DCHECK_GT(mcu_cols, 0);
  int mcu_rows = parse_result.frame_header.coded_height / (max_v_factor * 8);
  DCHECK_GT(mcu_rows, 0);
  slice_param->num_mcus = mcu_rows * mcu_cols;
}

// VAAPI only supports a subset of JPEG profiles. This function determines
// whether a given parsed JPEG result is supported or not.
static bool IsVaapiSupportedJpeg(const JpegParseResult& jpeg) {
  // Validate the visible size.
  if (jpeg.frame_header.visible_width == 0u) {
    DLOG(ERROR) << "Visible width can't be zero";
    return false;
  }
  if (jpeg.frame_header.visible_height == 0u) {
    DLOG(ERROR) << "Visible height can't be zero";
    return false;
  }

  // Validate the coded size.
  gfx::Size min_jpeg_resolution;
  if (!VaapiWrapper::GetJpegDecodeMinResolution(&min_jpeg_resolution)) {
    DLOG(ERROR) << "Could not get the minimum resolution";
    return false;
  }
  gfx::Size max_jpeg_resolution;
  if (!VaapiWrapper::GetJpegDecodeMaxResolution(&max_jpeg_resolution)) {
    DLOG(ERROR) << "Could not get the maximum resolution";
    return false;
  }
  const int actual_jpeg_coded_width =
      base::strict_cast<int>(jpeg.frame_header.coded_width);
  const int actual_jpeg_coded_height =
      base::strict_cast<int>(jpeg.frame_header.coded_height);
  if (actual_jpeg_coded_width < min_jpeg_resolution.width() ||
      actual_jpeg_coded_height < min_jpeg_resolution.height() ||
      actual_jpeg_coded_width > max_jpeg_resolution.width() ||
      actual_jpeg_coded_height > max_jpeg_resolution.height()) {
    DLOG(ERROR) << "VAAPI doesn't support size " << actual_jpeg_coded_width
                << "x" << actual_jpeg_coded_height << ": not in range "
                << min_jpeg_resolution.ToString() << " - "
                << max_jpeg_resolution.ToString();
    return false;
  }

  if (jpeg.frame_header.num_components != 3) {
    DLOG(ERROR) << "VAAPI doesn't support num_components("
                << static_cast<int>(jpeg.frame_header.num_components)
                << ") != 3";
    return false;
  }

  if (jpeg.frame_header.components[0].horizontal_sampling_factor <
          jpeg.frame_header.components[1].horizontal_sampling_factor ||
      jpeg.frame_header.components[0].horizontal_sampling_factor <
          jpeg.frame_header.components[2].horizontal_sampling_factor) {
    DLOG(ERROR) << "VAAPI doesn't supports horizontal sampling factor of Y"
                << " smaller than Cb and Cr";
    return false;
  }

  if (jpeg.frame_header.components[0].vertical_sampling_factor <
          jpeg.frame_header.components[1].vertical_sampling_factor ||
      jpeg.frame_header.components[0].vertical_sampling_factor <
          jpeg.frame_header.components[2].vertical_sampling_factor) {
    DLOG(ERROR) << "VAAPI doesn't supports vertical sampling factor of Y"
                << " smaller than Cb and Cr";
    return false;
  }

  return true;
}

// Convert the specified surface format to the associated output image format.
static bool VaSurfaceFormatToImageFormat(unsigned int va_rt_format,
                                         VAImageFormat* va_image_format) {
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      *va_image_format = kImageFormatI420;
      return true;
    case VA_RT_FORMAT_YUV422:
      *va_image_format = kImageFormatYUYV;
      return true;
    default:
      return false;
  }
}

static unsigned int VaSurfaceFormatForJpeg(
    const JpegFrameHeader& frame_header) {
  // The range of sampling factor is [1, 4]. Pack them into integer to make the
  // matching code simpler. For example, 0x211 means the sampling factor are 2,
  // 1, 1 for 3 components.
  unsigned int h = 0, v = 0;
  for (int i = 0; i < frame_header.num_components; i++) {
    DCHECK_LE(frame_header.components[i].horizontal_sampling_factor, 4);
    DCHECK_LE(frame_header.components[i].vertical_sampling_factor, 4);
    h = h << 4 | frame_header.components[i].horizontal_sampling_factor;
    v = v << 4 | frame_header.components[i].vertical_sampling_factor;
  }

  switch (frame_header.num_components) {
    case 1:  // Grey image
      return VA_RT_FORMAT_YUV400;

    case 3:  // Y Cb Cr color image
      // See https://en.wikipedia.org/wiki/Chroma_subsampling for the
      // definition of these numbers.
      if (h == 0x211 && v == 0x211)
        return VA_RT_FORMAT_YUV420;

      if (h == 0x211 && v == 0x111)
        return VA_RT_FORMAT_YUV422;

      if (h == 0x111 && v == 0x111)
        return VA_RT_FORMAT_YUV444;

      if (h == 0x411 && v == 0x111)
        return VA_RT_FORMAT_YUV411;
  }
  VLOGF(1) << "Unsupported sampling factor: num_components="
           << frame_header.num_components << ", h=" << std::hex << h
           << ", v=" << v;

  return kInvalidVaRtFormat;
}

}  // namespace

VaapiJpegDecoder::VaapiJpegDecoder()
    : va_surface_id_(VA_INVALID_SURFACE), va_rt_format_(kInvalidVaRtFormat) {}

VaapiJpegDecoder::~VaapiJpegDecoder() = default;

bool VaapiJpegDecoder::Initialize(const base::RepeatingClosure& error_uma_cb) {
  vaapi_wrapper_ = VaapiWrapper::Create(VaapiWrapper::kDecode,
                                        VAProfileJPEGBaseline, error_uma_cb);
  if (!vaapi_wrapper_) {
    VLOGF(1) << "Failed initializing VAAPI";
    return false;
  }
  return true;
}

std::unique_ptr<ScopedVAImage> VaapiJpegDecoder::DoDecode(
    base::span<const uint8_t> encoded_image,
    VaapiJpegDecodeStatus* status) {
  if (!vaapi_wrapper_) {
    VLOGF(1) << "VaapiJpegDecoder has not been initialized";
    *status = VaapiJpegDecodeStatus::kInvalidState;
    return nullptr;
  }

  // Parse the JPEG encoded data.
  JpegParseResult parse_result;
  if (!ParseJpegPicture(encoded_image.data(), encoded_image.size(),
                        &parse_result)) {
    VLOGF(1) << "ParseJpegPicture failed";
    *status = VaapiJpegDecodeStatus::kParseJpegFailed;
    return nullptr;
  }

  // Figure out the right format for the VaSurface.
  const unsigned int picture_va_rt_format =
      VaSurfaceFormatForJpeg(parse_result.frame_header);
  if (picture_va_rt_format == kInvalidVaRtFormat) {
    VLOGF(1) << "Unsupported subsampling";
    *status = VaapiJpegDecodeStatus::kUnsupportedSubsampling;
    return nullptr;
  }

  // Make sure this JPEG can be decoded.
  if (!IsVaapiSupportedJpeg(parse_result)) {
    VLOGF(1) << "The supplied JPEG is unsupported";
    *status = VaapiJpegDecodeStatus::kUnsupportedJpeg;
    return nullptr;
  }

  // Prepare the VaSurface for decoding.
  const gfx::Size new_coded_size(
      base::strict_cast<int>(parse_result.frame_header.coded_width),
      base::strict_cast<int>(parse_result.frame_header.coded_height));
  if (new_coded_size != coded_size_ || va_surface_id_ == VA_INVALID_SURFACE ||
      picture_va_rt_format != va_rt_format_) {
    vaapi_wrapper_->DestroyContextAndSurfaces();
    va_surface_id_ = VA_INVALID_SURFACE;
    va_rt_format_ = picture_va_rt_format;

    std::vector<VASurfaceID> va_surfaces;
    if (!vaapi_wrapper_->CreateContextAndSurfaces(va_rt_format_, new_coded_size,
                                                  1, &va_surfaces)) {
      VLOGF(1) << "Could not create the context or the surface";
      *status = VaapiJpegDecodeStatus::kSurfaceCreationFailed;
      return nullptr;
    }
    va_surface_id_ = va_surfaces[0];
    coded_size_ = new_coded_size;
  }

  // Set picture parameters.
  VAPictureParameterBufferJPEGBaseline pic_param{};
  FillPictureParameters(parse_result.frame_header, &pic_param);
  if (!vaapi_wrapper_->SubmitBuffer(VAPictureParameterBufferType, &pic_param)) {
    VLOGF(1) << "Could not submit VAPictureParameterBufferType";
    *status = VaapiJpegDecodeStatus::kSubmitPicParamsFailed;
    return nullptr;
  }

  // Set quantization table.
  VAIQMatrixBufferJPEGBaseline iq_matrix{};
  FillIQMatrix(parse_result.q_table, &iq_matrix);
  if (!vaapi_wrapper_->SubmitBuffer(VAIQMatrixBufferType, &iq_matrix)) {
    VLOGF(1) << "Could not submit VAIQMatrixBufferType";
    *status = VaapiJpegDecodeStatus::kSubmitIQMatrixFailed;
    return nullptr;
  }

  // Set huffman table.
  VAHuffmanTableBufferJPEGBaseline huffman_table{};
  FillHuffmanTable(parse_result.dc_table, parse_result.ac_table,
                   &huffman_table);
  if (!vaapi_wrapper_->SubmitBuffer(VAHuffmanTableBufferType, &huffman_table)) {
    VLOGF(1) << "Could not submit VAHuffmanTableBufferType";
    *status = VaapiJpegDecodeStatus::kSubmitHuffmanFailed;
    return nullptr;
  }

  // Set slice parameters.
  VASliceParameterBufferJPEGBaseline slice_param{};
  FillSliceParameters(parse_result, &slice_param);
  if (!vaapi_wrapper_->SubmitBuffer(VASliceParameterBufferType, &slice_param)) {
    VLOGF(1) << "Could not submit VASliceParameterBufferType";
    *status = VaapiJpegDecodeStatus::kSubmitSliceParamsFailed;
    return nullptr;
  }

  // Set scan data.
  if (!vaapi_wrapper_->SubmitBuffer(VASliceDataBufferType,
                                    parse_result.data_size,
                                    const_cast<char*>(parse_result.data))) {
    VLOGF(1) << "Could not submit VASliceDataBufferType";
    *status = VaapiJpegDecodeStatus::kSubmitSliceDataFailed;
    return nullptr;
  }

  // Execute the decode.
  if (!vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(va_surface_id_)) {
    VLOGF(1) << "Executing the decode failed";
    *status = VaapiJpegDecodeStatus::kExecuteDecodeFailed;
    return nullptr;
  }

  // Specify which image format we will request from the VAAPI. As the expected
  // output format is I420, we will first try this format. If converting to I420
  // is not supported by the decoder, we will request the image in its original
  // chroma sampling format.
  VAImageFormat va_image_format = kImageFormatI420;
  if (!VaapiWrapper::IsImageFormatSupported(va_image_format) &&
      !VaSurfaceFormatToImageFormat(va_rt_format_, &va_image_format)) {
    VLOGF(1) << "Unsupported surface format";
    *status = VaapiJpegDecodeStatus::kUnsupportedSurfaceFormat;
    return nullptr;
  }

  auto scoped_image = vaapi_wrapper_->CreateVaImage(
      va_surface_id_, &va_image_format, coded_size_);
  if (!scoped_image) {
    VLOGF(1) << "Cannot get VAImage";
    *status = VaapiJpegDecodeStatus::kCannotGetImage;
    return nullptr;
  }

  DCHECK_EQ(va_image_format.fourcc, scoped_image->image()->format.fourcc);
  *status = VaapiJpegDecodeStatus::kSuccess;
  return scoped_image;
}

}  // namespace media
