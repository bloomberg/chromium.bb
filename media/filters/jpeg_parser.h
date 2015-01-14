// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef MEDIA_FILTERS_JPEG_PARSER_H_
#define MEDIA_FILTERS_JPEG_PARSER_H_

#include <stddef.h>
#include <stdint.h>
#include "media/base/media_export.h"

namespace media {

const size_t kJpegMaxHuffmanTableNumBaseline = 2;
const size_t kJpegMaxComponents = 4;
const size_t kJpegMaxQuantizationTableNum = 4;

// Parsing result of JPEG DHT marker.
struct JpegHuffmanTable {
  bool valid;
  uint8_t code_length[16];
  uint8_t code_value[256];
};

// Parsing result of JPEG DQT marker.
struct JpegQuantizationTable {
  bool valid;
  uint8_t value[64];  // baseline only supports 8 bits quantization table
};

// Parsing result of a JPEG component.
struct JpegComponent {
  uint8_t id;
  uint8_t horizontal_sampling_factor;
  uint8_t vertical_sampling_factor;
  uint8_t quantization_table_selector;
};

// Parsing result of a JPEG SOF marker.
struct JpegFrameHeader {
  uint16_t visible_width;
  uint16_t visible_height;
  uint8_t num_components;
  JpegComponent components[kJpegMaxComponents];
};

// Parsing result of JPEG SOS marker.
struct JpegScanHeader {
  uint8_t num_components;
  struct Component {
    uint8_t component_selector;
    uint8_t dc_selector;
    uint8_t ac_selector;
  } components[kJpegMaxComponents];
};

struct JpegParseResult {
  JpegFrameHeader frame_header;
  JpegHuffmanTable dc_table[kJpegMaxHuffmanTableNumBaseline];
  JpegHuffmanTable ac_table[kJpegMaxHuffmanTableNumBaseline];
  JpegQuantizationTable q_table[kJpegMaxQuantizationTableNum];
  uint16_t restart_interval;
  JpegScanHeader scan;
  const char* data;
  size_t data_size;
};

// Parses JPEG picture in |buffer| with |length|.  Returns true iff header is
// valid and JPEG baseline sequential process is present. If parsed
// successfully, |result| is the parsed result.
// It's not a full featured JPEG parser implememtation. It only parses JPEG
// baseline sequential process. For explanations of each struct and its
// members, see JPEG specification at
// http://www.w3.org/Graphics/JPEG/itu-t81.pdf.
MEDIA_EXPORT bool ParseJpegPicture(const uint8_t* buffer,
                                   size_t length,
                                   JpegParseResult* result);

}  // namespace media

#endif  // MEDIA_FILTERS_JPEG_PARSER_H_
