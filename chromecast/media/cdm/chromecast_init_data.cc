// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/chromecast_init_data.h"

#include "base/logging.h"
#include "media/base/bit_reader.h"

namespace chromecast {
namespace media {

#define RCHECK(x)   \
  do {              \
    if (!(x))       \
      return false; \
  } while (0)

namespace {

const uint8_t kChromecastPlayreadyUuid[] = {
    0x2b, 0xf8, 0x66, 0x80, 0xc6, 0xe5, 0x4e, 0x24,
    0xbe, 0x23, 0x0f, 0x81, 0x5a, 0x60, 0x6e, 0xb2};

// ASCII "uuid" as an 4-byte integer
const uint32_t kBoxTypeUuid = 0x75756964;
// ASCII "pssh" as an 4-byte integer
const uint32_t kBoxTypePssh = 0x70737368;

bool IsChromecastSystemId(::media::BitReader& reader) {
  for (size_t i = 0; i < arraysize(kChromecastPlayreadyUuid); ++i) {
    uint8_t one_byte;
    RCHECK(reader.ReadBits(8, &one_byte));
    if (one_byte != kChromecastPlayreadyUuid[i])
      return false;
  }
  return true;
}

bool ParseLegacyUuidBox(::media::BitReader& reader,
                        ChromecastInitData* chromecast_init_data_out) {
  // See b/10246367 for format.
  RCHECK(IsChromecastSystemId(reader));
  chromecast_init_data_out->type = InitDataMessageType::CUSTOM_DATA;
  size_t custom_data_size = reader.bits_available() / 8;
  chromecast_init_data_out->data.resize(custom_data_size);
  for (size_t i = 0; i < custom_data_size; ++i)
    RCHECK(reader.ReadBits(8, &chromecast_init_data_out->data[i]));
  return true;
}

}  // namespace

ChromecastInitData::ChromecastInitData() {
}

ChromecastInitData::~ChromecastInitData() {
}

bool FindChromecastInitData(const std::vector<uint8_t>& init_data,
                            InitDataMessageType type,
                            ChromecastInitData* chromecast_init_data_out) {
  // Chromecast initData assumes a CENC data format and searches for PSSH boxes
  // with SystemID |kChromecastPlayreadyUuid|. The PSSH box content is as
  // follows:
  // * |type| (2 bytes, InitDataMessageType)
  // * |data| (all remaining bytes)
  // Data may or may not be present and is specific to the given |type|.
  ::media::BitReader reader(&init_data[0], init_data.size());
  while (reader.bits_available() > 64) {
    uint32_t box_size;
    uint32_t box_type;
    reader.ReadBits(4 * 8, &box_size);
    reader.ReadBits(4 * 8, &box_type);
    int box_bytes_read = 8;

    // TODO(gunsch): uuid boxes in initData are no longer supported. This should
    // be removed when apps have updated.
    if (box_type == kBoxTypeUuid) {
      ::media::BitReader uuidReader(&init_data[reader.bits_read() / 8],
                                    box_size - box_bytes_read);
      if (type == InitDataMessageType::CUSTOM_DATA &&
          ParseLegacyUuidBox(uuidReader, chromecast_init_data_out)) {
        return true;
      }

      // Box size includes the bytes already consumed
      reader.SkipBits((box_size - box_bytes_read) * 8);
      continue;
    }

    if (box_type != kBoxTypePssh) {
      reader.SkipBits((box_size - box_bytes_read) * 8);
      continue;
    }

    // 8-bit version, 24-bit flags
    RCHECK(reader.SkipBits(4 * 8));
    box_bytes_read += 4;

    ::media::BitReader systemIdReader(&init_data[reader.bits_read() / 8],
                                      box_size - box_bytes_read);
    if (!IsChromecastSystemId(systemIdReader)) {
      reader.SkipBits((box_size - box_bytes_read) * 8);
      continue;
    }
    RCHECK(reader.SkipBits(16 * 8));
    box_bytes_read += 16;

    uint16_t msg_type;
    RCHECK(reader.ReadBits(2 * 8, &msg_type));
    box_bytes_read += 2;
    RCHECK(msg_type < static_cast<uint16_t>(InitDataMessageType::END));

    chromecast_init_data_out->type = static_cast<InitDataMessageType>(msg_type);
    const uint8_t* data_start = &init_data[0] + reader.bits_read() / 8;
    chromecast_init_data_out->data.assign(
        data_start, data_start + box_size - box_bytes_read);
    return true;
  }
  return false;
}

}  // namespace media
}  // namespace chromecast
