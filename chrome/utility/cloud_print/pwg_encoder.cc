// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/cloud_print/pwg_encoder.h"

#include <algorithm>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/utility/cloud_print/bitmap_image.h"

namespace cloud_print {

namespace {

const uint32 kBitsPerColor = 8;
const uint32 kColorOrder = 0;  // chunky.

// Coefficients used to convert from RGB to monochrome.
const uint32 kRedCoefficient = 2125;
const uint32 kGreenCoefficient = 7154;
const uint32 kBlueCoefficient = 721;
const uint32 kColorCoefficientDenominator = 10000;

const char* kPwgKeyword = "RaS2";

const uint32 kHeaderSize = 1796;
const uint32 kHeaderCupsDuplex = 272;
const uint32 kHeaderCupsHwResolutionHorizontal = 276;
const uint32 kHeaderCupsHwResolutionVertical = 280;
const uint32 kHeaderCupsTumble = 368;
const uint32 kHeaderCupsWidth = 372;
const uint32 kHeaderCupsHeight = 376;
const uint32 kHeaderCupsBitsPerColor = 384;
const uint32 kHeaderCupsBitsPerPixel = 388;
const uint32 kHeaderCupsBytesPerLine = 392;
const uint32 kHeaderCupsColorOrder = 396;
const uint32 kHeaderCupsColorSpace = 400;
const uint32 kHeaderCupsNumColors = 420;
const uint32 kHeaderPwgTotalPageCount = 452;
const uint32 kHeaderPwgCrossFeedTransform = 456;
const uint32 kHeaderPwgFeedTransform = 460;

const int kPwgMaxPackedRows = 256;

const int kPwgMaxPackedPixels = 128;

struct RGBA8 {
  uint8 red;
  uint8 green;
  uint8 blue;
  uint8 alpha;
};

struct BGRA8 {
  uint8 blue;
  uint8 green;
  uint8 red;
  uint8 alpha;
};

template <class InputStruct>
inline void encodePixelToRGB(const void* pixel, std::string* output) {
  const InputStruct* i = reinterpret_cast<const InputStruct*>(pixel);
  output->push_back(static_cast<char>(i->red));
  output->push_back(static_cast<char>(i->green));
  output->push_back(static_cast<char>(i->blue));
}

template <class InputStruct>
inline void encodePixelToMonochrome(const void* pixel, std::string* output) {
  const InputStruct* i = reinterpret_cast<const InputStruct*>(pixel);
  output->push_back(static_cast<char>((i->red * kRedCoefficient +
                                       i->green * kGreenCoefficient +
                                       i->blue * kBlueCoefficient) /
                                      kColorCoefficientDenominator));
}

}  // namespace

PwgEncoder::PwgEncoder() {}

void PwgEncoder::EncodeDocumentHeader(std::string* output) const {
  output->clear();
  output->append(kPwgKeyword, 4);
}

void PwgEncoder::EncodePageHeader(const BitmapImage& image,
                                  const PwgHeaderInfo& pwg_header_info,
                                  std::string* output) const {
  char header[kHeaderSize];
  memset(header, 0, kHeaderSize);

  uint32 num_colors =
      pwg_header_info.color_space == PwgHeaderInfo::SGRAY ? 1 : 3;
  uint32 bits_per_pixel = num_colors * kBitsPerColor;

  base::WriteBigEndian<uint32>(header + kHeaderCupsDuplex,
                               pwg_header_info.duplex ? 1 : 0);
  base::WriteBigEndian<uint32>(header + kHeaderCupsHwResolutionHorizontal,
                               pwg_header_info.dpi);
  base::WriteBigEndian<uint32>(header + kHeaderCupsHwResolutionVertical,
                               pwg_header_info.dpi);
  base::WriteBigEndian<uint32>(header + kHeaderCupsTumble,
                               pwg_header_info.tumble ? 1 : 0);
  base::WriteBigEndian<uint32>(header + kHeaderCupsWidth, image.size().width());
  base::WriteBigEndian<uint32>(header + kHeaderCupsHeight,
                               image.size().height());
  base::WriteBigEndian<uint32>(header + kHeaderCupsBitsPerColor, kBitsPerColor);
  base::WriteBigEndian<uint32>(header + kHeaderCupsBitsPerPixel,
                               bits_per_pixel);
  base::WriteBigEndian<uint32>(header + kHeaderCupsBytesPerLine,
                               (bits_per_pixel * image.size().width() + 7) / 8);
  base::WriteBigEndian<uint32>(header + kHeaderCupsColorOrder, kColorOrder);
  base::WriteBigEndian<uint32>(header + kHeaderCupsColorSpace,
                               pwg_header_info.color_space);
  base::WriteBigEndian<uint32>(header + kHeaderCupsNumColors, num_colors);
  base::WriteBigEndian<uint32>(header + kHeaderPwgCrossFeedTransform,
                               pwg_header_info.flipx ? -1 : 1);
  base::WriteBigEndian<uint32>(header + kHeaderPwgFeedTransform,
                               pwg_header_info.flipy ? -1 : 1);
  base::WriteBigEndian<uint32>(header + kHeaderPwgTotalPageCount,
                               pwg_header_info.total_pages);
  output->append(header, kHeaderSize);
}

template <typename InputStruct, class RandomAccessIterator>
void PwgEncoder::EncodeRow(RandomAccessIterator pos,
                           RandomAccessIterator row_end,
                           bool monochrome,
                           std::string* output) const {
  // According to PWG-raster, a sequence of N identical pixels (up to 128)
  // can be encoded by a byte N-1, followed by the information on
  // that pixel. Any generic sequence of N pixels (up to 129) can be encoded
  // with (signed) byte 1-N, followed by the information on the N pixels.
  // Notice that for sequences of 1 pixel there is no difference between
  // the two encodings.

  // We encode every largest sequence of identical pixels together because it
  // usually saves the most space. Every other pixel should be encoded in the
  // smallest number of generic sequences.
  // NOTE: the algorithm is not optimal especially in case of monochrome.
  while (pos != row_end) {
    RandomAccessIterator it = pos + 1;
    RandomAccessIterator end = std::min(pos + kPwgMaxPackedPixels, row_end);

    // Counts how many identical pixels (up to 128).
    while (it != end && *pos == *it) {
      ++it;
    }
    if (it != pos + 1) {  // More than one pixel
      output->push_back(static_cast<char>((it - pos) - 1));
      if (monochrome)
        encodePixelToMonochrome<InputStruct>(&*pos, output);
      else
        encodePixelToRGB<InputStruct>(&*pos, output);
      pos = it;
    } else {
      // Finds how many pixels there are each different from the previous one.
      // IMPORTANT: even if sequences of different pixels can contain as many
      // as 129 pixels, we restrict to 128 because some decoders don't manage
      // it correctly. So iterating until it != end is correct.
      while (it != end && *it != *(it - 1)) {
        ++it;
      }
      // Optimization: ignores the last pixel of the sequence if it is followed
      // by an identical pixel, as it is more convenient for it to be the start
      // of a new sequence of identical pixels. Notice that we don't compare
      // to end, but row_end.
      if (it != row_end && *it == *(it - 1)) {
        --it;
      }
      output->push_back(static_cast<char>(1 - (it - pos)));
      while (pos != it) {
        if (monochrome)
          encodePixelToMonochrome<InputStruct>(&*pos, output);
        else
          encodePixelToRGB<InputStruct>(&*pos, output);
        ++pos;
      }
    }
  }
}

inline const uint8* PwgEncoder::GetRow(const BitmapImage& image,
                                       int row,
                                       bool flipy) const {
  return image.GetPixel(
      gfx::Point(0, flipy ? image.size().height() - 1 - row : row));
}

// Given a pointer to a struct Image, create a PWG of the image and
// put the compressed image data in the string.  Returns true on success.
// The content of the string is undefined on failure.
bool PwgEncoder::EncodePage(const BitmapImage& image,
                            const PwgHeaderInfo& pwg_header_info,
                            std::string* output) const {
  // pwg_header_info.color_space can only contain color spaces that are
  // supported, so no sanity check is needed.
  switch (image.colorspace()) {
    case BitmapImage::RGBA:
      return EncodePageWithColorspace<RGBA8>(image, pwg_header_info, output);

    case BitmapImage::BGRA:
      return EncodePageWithColorspace<BGRA8>(image, pwg_header_info, output);

    default:
      LOG(ERROR) << "Unsupported colorspace.";
      return false;
  }
}

template <typename InputStruct>
bool PwgEncoder::EncodePageWithColorspace(const BitmapImage& image,
                                          const PwgHeaderInfo& pwg_header_info,
                                          std::string* output) const {
  bool monochrome = pwg_header_info.color_space == PwgHeaderInfo::SGRAY;
  EncodePageHeader(image, pwg_header_info, output);

  // Ensure no integer overflow.
  CHECK(image.size().width() < INT_MAX / image.channels());
  int row_size = image.size().width() * image.channels();

  int row_number = 0;
  while (row_number < image.size().height()) {
    const uint8* current_row =
        GetRow(image, row_number++, pwg_header_info.flipy);
    int num_identical_rows = 1;
    // We count how many times the current row is repeated.
    while (num_identical_rows < kPwgMaxPackedRows &&
           row_number < image.size().height() &&
           !memcmp(current_row,
                   GetRow(image, row_number, pwg_header_info.flipy),
                   row_size)) {
      num_identical_rows++;
      row_number++;
    }
    output->push_back(static_cast<char>(num_identical_rows - 1));

    // Both supported colorspaces have a 32-bit pixels information.
    // Converts the list of uint8 to uint32 as every pixels contains 4 bytes
    // of information and comparison of elements is easier. The actual
    // Management of the bytes of the pixel is done by pixel_encoder function
    // on the original array to avoid endian problems.
    const uint32* pos = reinterpret_cast<const uint32*>(current_row);
    const uint32* row_end = pos + image.size().width();
    if (!pwg_header_info.flipx) {
      EncodeRow<InputStruct>(pos, row_end, monochrome, output);
    } else {
      // We reverse the iterators.
      EncodeRow<InputStruct>(std::reverse_iterator<const uint32*>(row_end),
                             std::reverse_iterator<const uint32*>(pos),
                             monochrome,
                             output);
    }
  }
  return true;
}

}  // namespace cloud_print
