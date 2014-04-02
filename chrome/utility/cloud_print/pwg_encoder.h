// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CLOUD_PRINT_PWG_ENCODER_H_
#define CHROME_UTILITY_CLOUD_PRINT_PWG_ENCODER_H_

#include <string>

#include "base/basictypes.h"

namespace cloud_print {

class BitmapImage;

class PwgEncoder {
 public:
  PwgEncoder();

  void EncodeDocumentHeader(std::string *output) const;
  bool EncodePage(const BitmapImage& image,
                  const uint32 dpi,
                  const uint32 total_pages,
                  std::string* output,
                  bool rotate) const;

 private:
  void EncodePageHeader(const BitmapImage& image, const uint32 dpi,
                        const uint32 total_pages, std::string* output) const;
  bool EncodeRowFrom32Bit(const uint8* row,
                          const int width,
                          const int color_space,
                          std::string* output) const;
  const uint8* GetRow(const BitmapImage& image, int row) const;
};

}  // namespace cloud_print

#endif  // CHROME_UTILITY_CLOUD_PRINT_PWG_ENCODER_H_
