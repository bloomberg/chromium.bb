// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CLOUD_PRINT_PWG_ENCODER_H_
#define CHROME_UTILITY_CLOUD_PRINT_PWG_ENCODER_H_

#include <string>

#include "base/basictypes.h"

namespace cloud_print {

class BitmapImage;

struct PwgHeaderInfo {
  PwgHeaderInfo()
      : dpi(300),
        total_pages(1),
        flipx(false),
        flipy(false),
        color_space(SRGB),
        duplex(false),
        tumble(false) {}
  enum ColorSpace { SGRAY = 18, SRGB = 19 };
  uint32 dpi;
  uint32 total_pages;
  bool flipx;
  bool flipy;
  ColorSpace color_space;
  bool duplex;
  bool tumble;
};

class PwgEncoder {
 public:
  PwgEncoder();

  void EncodeDocumentHeader(std::string *output) const;
  bool EncodePage(const BitmapImage& image,
                  const PwgHeaderInfo& pwg_header_info,
                  std::string* output) const;

 private:
  void EncodePageHeader(const BitmapImage& image,
                        const PwgHeaderInfo& pwg_header_info,
                        std::string* output) const;

  template <typename InputStruct, class RandomAccessIterator>
  void EncodeRow(RandomAccessIterator pos,
                 RandomAccessIterator row_end,
                 bool monochrome,
                 std::string* output) const;

  template <typename InputStruct>
  bool EncodePageWithColorspace(const BitmapImage& image,
                                const PwgHeaderInfo& pwg_header_info,
                                std::string* output) const;

  const uint8* GetRow(const BitmapImage& image, int row, bool flipy) const;
};

}  // namespace cloud_print

#endif  // CHROME_UTILITY_CLOUD_PRINT_PWG_ENCODER_H_
