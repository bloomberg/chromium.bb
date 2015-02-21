// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cenc_utils.h"

#include "media/base/bit_reader.h"

namespace media {

// The initialization data for encrypted media files using the ISO Common
// Encryption ('cenc') protection scheme may contain one or more protection
// system specific header ('pssh') boxes.
// ref: https://w3c.github.io/encrypted-media/cenc-format.html
//
// The format of a 'pssh' box is as follows:
//   unsigned int(32) size;
//   unsigned int(32) type = "pssh";
//   if (size==1) {
//     unsigned int(64) largesize;
//   } else if (size==0) {
//     -- box extends to end of file
//   }
//   unsigned int(8) version;
//   bit(24) flags;
//   unsigned int(8)[16] SystemID;
//   if (version > 0)
//   {
//     unsigned int(32) KID_count;
//     {
//       unsigned int(8)[16] KID;
//     } [KID_count]
//   }
//   unsigned int(32) DataSize;
//   unsigned int(8)[DataSize] Data;

// Minimum size of a 'pssh' box includes  all the required fields (size, type,
// version, flags, SystemID, DataSize).
const int kMinimumBoxSizeInBytes = 32;

// SystemID for the Common System.
// https://w3c.github.io/encrypted-media/cenc-format.html#common-system
const uint8 kCommonSystemId[] = { 0x10, 0x77, 0xef, 0xec,
                                  0xc0, 0xb2, 0x4d, 0x02,
                                  0xac, 0xe3, 0x3c, 0x1e,
                                  0x52, 0xe2, 0xfb, 0x4b };

#define RCHECK(x)   \
  do {              \
    if (!(x))       \
      return false; \
  } while (0)

// Helper function to read up to 32 bits from a bit stream.
static uint32 ReadBits(BitReader* reader, int num_bits) {
  DCHECK_GE(reader->bits_available(), num_bits);
  DCHECK((num_bits > 0) && (num_bits <= 32));
  uint32 value;
  reader->ReadBits(num_bits, &value);
  return value;
}

// Checks whether the next 16 bytes matches the Common SystemID.
// Assumes |reader| has enough data.
static bool IsCommonSystemID(BitReader* reader) {
  for (uint32 i = 0; i < arraysize(kCommonSystemId); ++i) {
    if (ReadBits(reader, 8) != kCommonSystemId[i])
      return false;
  }
  return true;
}

bool GetKeyIdsForCommonSystemId(const uint8* input,
                                int input_length,
                                std::vector<std::vector<uint8>>* key_ids) {
  int offset = 0;
  std::vector<std::vector<uint8>> result;

  while (offset < input_length) {
    BitReader reader(input + offset, input_length - offset);

    // Enough data for a miniumum size 'pssh' box?
    RCHECK(reader.bits_available() >= kMinimumBoxSizeInBytes * 8);

    uint32 size = ReadBits(&reader, 32);

    // Must be a 'pssh' box or else fail.
    RCHECK(ReadBits(&reader, 8) == 'p');
    RCHECK(ReadBits(&reader, 8) == 's');
    RCHECK(ReadBits(&reader, 8) == 's');
    RCHECK(ReadBits(&reader, 8) == 'h');

    if (size == 1) {
      // If largesize > 2**32 it is too big.
      RCHECK(ReadBits(&reader, 32) == 0);
      size = ReadBits(&reader, 32);
    } else if (size == 0) {
      size = input_length - offset;
    }

    // Check that the buffer contains at least size bytes.
    RCHECK(static_cast<uint32>(input_length - offset) >= size);

    // Update offset to point at the next 'pssh' box (may not be one).
    offset += size;

    // Check the version, as KIDs only available if version > 0.
    uint8 version = ReadBits(&reader, 8);
    if (version == 0)
      continue;

    // flags must be 0. If not, assume incorrect 'pssh' box and move to the
    // next one.
    if (ReadBits(&reader, 24) != 0)
      continue;

    // Validate SystemID
    RCHECK(static_cast<uint32>(reader.bits_available()) >=
           arraysize(kCommonSystemId) * 8);
    if (!IsCommonSystemID(&reader))
      continue;  // Not Common System, so try the next pssh box.

    // Since version > 0, next field is the KID_count.
    RCHECK(static_cast<uint32>(reader.bits_available()) >= sizeof(uint32) * 8);
    uint32 count = ReadBits(&reader, 32);

    if (count == 0)
      continue;

    // Make sure there is enough data for all the KIDs specified, and then
    // extract them.
    RCHECK(static_cast<uint32>(reader.bits_available()) > count * 16 * 8);
    while (count > 0) {
      std::vector<uint8> key;
      key.reserve(16);
      for (int i = 0; i < 16; ++i) {
        key.push_back(ReadBits(&reader, 8));
      }
      result.push_back(key);
      --count;
    }

    // Don't bother checking DataSize and Data.
  }

  key_ids->swap(result);

  // TODO(jrummell): This should return true only if there was at least one
  // key ID present. However, numerous test files don't contain the 'pssh' box
  // for Common Format, so no keys are found. http://crbug.com/460308
  return true;
}

}  // namespace media
