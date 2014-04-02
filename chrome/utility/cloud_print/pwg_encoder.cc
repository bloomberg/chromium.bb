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
const uint32 kColorSpace = 19;  // sRGB.
const uint32 kColorOrder = 0;  // chunky.
const uint32 kNumColors = 3;
const uint32 kBitsPerPixel = kNumColors * kBitsPerColor;

const char kPwgKeyword[] = "RaS2";

const uint32 kHeaderSize = 1796;
const uint32 kHeaderHwResolutionHorizontal = 276;
const uint32 kHeaderHwResolutionVertical = 280;
const uint32 kHeaderCupsWidth = 372;
const uint32 kHeaderCupsHeight = 376;
const uint32 kHeaderCupsBitsPerColor = 384;
const uint32 kHeaderCupsBitsPerPixel = 388;
const uint32 kHeaderCupsBytesPerLine = 392;
const uint32 kHeaderCupsColorOrder = 396;
const uint32 kHeaderCupsColorSpace = 400;
const uint32 kHeaderCupsNumColors = 420;
const uint32 kHeaderPwgTotalPageCount = 452;

const int kPwgMaxPackedRows = 256;

const int kPwgMaxPackedPixels = 128;

inline int FlipIfNeeded(bool flip, int current, int total) {
  return flip ? total - current : current;
}

}  // namespace

PwgEncoder::PwgEncoder() {}

inline void encodePixelFromRGBA(const uint8* pixel, std::string* output) {
  output->push_back(static_cast<char>(pixel[0]));
  output->push_back(static_cast<char>(pixel[1]));
  output->push_back(static_cast<char>(pixel[2]));
}

inline void encodePixelFromBGRA(const uint8* pixel, std::string* output) {
  output->push_back(static_cast<char>(pixel[2]));
  output->push_back(static_cast<char>(pixel[1]));
  output->push_back(static_cast<char>(pixel[0]));
}

void PwgEncoder::EncodeDocumentHeader(std::string* output) const {
  output->clear();
  output->append(kPwgKeyword, 4);
}

void PwgEncoder::EncodePageHeader(const BitmapImage& image, const uint32 dpi,
                                  const uint32 total_pages,
                                  std::string* output) const {
  char header[kHeaderSize];
  memset(header, 0, kHeaderSize);
  base::WriteBigEndian<uint32>(header + kHeaderHwResolutionHorizontal, dpi);
  base::WriteBigEndian<uint32>(header + kHeaderHwResolutionVertical, dpi);
  base::WriteBigEndian<uint32>(header + kHeaderCupsWidth, image.size().width());
  base::WriteBigEndian<uint32>(header + kHeaderCupsHeight,
                               image.size().height());
  base::WriteBigEndian<uint32>(header + kHeaderCupsBitsPerColor, kBitsPerColor);
  base::WriteBigEndian<uint32>(header + kHeaderCupsBitsPerPixel, kBitsPerPixel);
  base::WriteBigEndian<uint32>(header + kHeaderCupsBytesPerLine,
                               (kBitsPerPixel * image.size().width() + 7) / 8);
  base::WriteBigEndian<uint32>(header + kHeaderCupsColorOrder, kColorOrder);
  base::WriteBigEndian<uint32>(header + kHeaderCupsColorSpace, kColorSpace);
  base::WriteBigEndian<uint32>(header + kHeaderCupsNumColors, kNumColors);
  base::WriteBigEndian<uint32>(header + kHeaderPwgTotalPageCount, total_pages);
  output->append(header, kHeaderSize);
}

bool PwgEncoder::EncodeRowFrom32Bit(const uint8* row, const int width,
                                    const int color_space,
                                    std::string* output) const {
  void (*pixel_encoder)(const uint8*, std::string*);
  switch (color_space) {
    case BitmapImage::RGBA:
      pixel_encoder = &encodePixelFromRGBA;
      break;
    case BitmapImage::BGRA:
      pixel_encoder = &encodePixelFromBGRA;
      break;
    default:
      LOG(ERROR) << "Unsupported colorspace.";
      return false;
  }

  // Converts the list of uint8 to uint32 as every pixels contains 4 bytes
  // of information and comparison of elements is easier. The actual management
  // of the bytes of the pixel is done by template function P on the original
  // array to avoid endian problems.
  const uint32* pos = reinterpret_cast<const uint32*>(row);
  const uint32* row_end = pos + width;
  // According to PWG-raster, a sequence of N identical pixels (up to 128)
  // can be encoded by a byte N-1, followed by the information on
  // that pixel. Any generic sequence of N pixels (up to 128) can be encoded
  // with (signed) byte 1-N, followed by the information on the N pixels.
  // Notice that for sequences of 1 pixel there is no difference between
  // the two encodings.

  // It is usually better to encode every largest sequence of > 2 identical
  // pixels together because it saves the most space. Every other pixel should
  // be encoded in the smallest number of generic sequences.
  while (pos != row_end) {
    const uint32* it = pos + 1;
    const uint32* end = std::min(pos + kPwgMaxPackedPixels, row_end);

    // Counts how many identical pixels (up to 128).
    while (it != end && *pos == *it) {
      it++;
    }
    if (it != pos + 1) {  // More than one pixel
      output->push_back(static_cast<char>((it - pos) - 1));
      pixel_encoder(reinterpret_cast<const uint8*>(pos), output);
      pos = it;
    } else {
      // Finds how many pixels each different from the previous one (up to 128).
      while (it != end && *it != *(it - 1)) {
        it++;
      }
      // Optimization: ignores the last pixel of the sequence if it is followed
      // by an identical pixel, as it is more convenient for it to be the start
      // of a new sequence of identical pixels. Notice that we don't compare
      // to end, but row_end.
      if (it != row_end && *it == *(it - 1)) {
        it--;
      }
      output->push_back(static_cast<char>(1 - (it - pos)));
      while (pos != it) {
        pixel_encoder(reinterpret_cast<const uint8*>(pos++), output);
      }
    }
  }
  return true;
}

inline const uint8* PwgEncoder::GetRow(const BitmapImage& image,
                                       int row) const {
  return image.pixel_data() + row * image.size().width() * image.channels();
}

// Given a pointer to a struct Image, create a PWG of the image and
// put the compressed image data in the std::string.  Returns true on success.
// The content of the std::string is undefined on failure.
bool PwgEncoder::EncodePage(const BitmapImage& image,
                            const uint32 dpi,
                            const uint32 total_pages,
                            std::string* output,
                            bool rotate) const {
  // For now only some 4-channel colorspaces are supported.
  if (image.channels() != 4) {
    LOG(ERROR) << "Unsupported colorspace.";
    return false;
  }

  EncodePageHeader(image, dpi, total_pages, output);

  int row_size = image.size().width() * image.channels();
  scoped_ptr<uint8[]> current_row_cpy(new uint8[row_size]);

  int row_number = 0;
  int total_rows = image.size().height();
  while (row_number < total_rows) {
    const uint8* current_row =
        GetRow(image, FlipIfNeeded(rotate, row_number++, total_rows));
    int num_identical_rows = 1;
    // We count how many times the current row is repeated.
    while (num_identical_rows < kPwgMaxPackedRows &&
           row_number < image.size().height() &&
           !memcmp(current_row,
                   GetRow(image, FlipIfNeeded(rotate, row_number, total_rows)),
                   row_size)) {
      num_identical_rows++;
      row_number++;
    }
    output->push_back(static_cast<char>(num_identical_rows - 1));

    if (rotate) {
      memcpy(current_row_cpy.get(), current_row, row_size);
      std::reverse(reinterpret_cast<uint32*>(current_row_cpy.get()),
                   reinterpret_cast<uint32*>(current_row_cpy.get() + row_size));
      current_row = current_row_cpy.get();
    }

    // Both supported colorspaces have a 32-bit pixels information.
    if (!EncodeRowFrom32Bit(
            current_row, image.size().width(), image.colorspace(), output)) {
      return false;
    }
  }
  return true;
}

}  // namespace cloud_print
