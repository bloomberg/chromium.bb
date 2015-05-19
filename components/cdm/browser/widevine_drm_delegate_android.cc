// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/browser/widevine_drm_delegate_android.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace cdm {

namespace {

uint32_t ReadUint32(const uint8_t* data) {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i)
    value = (value << 8) | data[i];
  return value;
}

uint64_t ReadUint64(const uint8_t* data) {
  uint64_t value = 0;
  for (int i = 0; i < 8; ++i)
    value = (value << 8) | data[i];
  return value;
}

// The structure of an ISO CENC Protection System Specific Header (PSSH) box is
// as follows. (See ISO/IEC FDIS 23001-7:2011(E).)
// Note: ISO boxes use big-endian values.
//
// PSSH {
//   uint32 Size
//   uint32 Type
//   uint64 LargeSize  # Field is only present if value(Size) == 1.
//   uint8 Version
//   uint24 Flags
//   uint8[16] SystemId
//   if (version > 0) {
//     uint32 KID_count;
//     uint8[16][KID_Count] KID;
//   }
//   uint32 DataSize
//   uint8[DataSize] Data
// }
const int kBoxHeaderSize = 8;  // Box's header contains Size and Type.
const int kBoxLargeSizeSize = 8;
const int kPsshVersionFlagSize = 4;
const uint32_t k24BitMask = 0x00ffffff;
const int kPsshSystemIdSize = 16;
const int kPsshKidCountSize = 4;
const int kPsshKidSize = 16;
const int kPsshDataSizeSize = 4;
const uint32_t kPsshType = 0x70737368;

const uint8_t kWidevineUuid[16] = {
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };

// Tries to find a PSSH box with the Widevine UUID, parses the
// "Data" of the box and put it in |pssh_data|. Returns true if such a box is
// found and successfully parsed. Returns false otherwise.
// Notes:
// 1, If multiple PSSH boxes are found,the "Data" of the first matching PSSH box
// will be set in |pssh_data|.
// 2, Only PSSH boxes are allowed in |data|.
bool GetPsshData(const std::vector<uint8_t>& data,
                 std::vector<uint8_t>* pssh_data) {
  int bytes_left = base::checked_cast<int>(data.size());
  const uint8_t* cur = &data[0];
  const uint8_t* data_end = cur + bytes_left;

  while (bytes_left > 0) {
    const uint8_t* box_head = cur;

    if (bytes_left < kBoxHeaderSize)
      return false;

    uint64_t box_size = ReadUint32(cur);
    uint32_t type = ReadUint32(cur + 4);
    cur += kBoxHeaderSize;
    bytes_left -= kBoxHeaderSize;

    if (box_size == 1) {  // LargeSize is present.
      if (bytes_left < kBoxLargeSizeSize)
        return false;

      box_size = ReadUint64(cur);
      cur += kBoxLargeSizeSize;
      bytes_left -= kBoxLargeSizeSize;
    } else if (box_size == 0) {
      box_size = bytes_left + kBoxHeaderSize;
    }

    const uint8_t* box_end = box_head + box_size;
    if (data_end < box_end)
      return false;

    if (type != kPsshType)
      return false;

    const int kPsshBoxMinimumSize =
        kPsshVersionFlagSize + kPsshSystemIdSize + kPsshDataSizeSize;
    if (box_end < cur + kPsshBoxMinimumSize)
      return false;

    uint8_t version = cur[0];
    uint32_t flags = ReadUint32(cur) & k24BitMask;
    cur += kPsshVersionFlagSize;
    bytes_left -= kPsshVersionFlagSize;
    if (flags != 0)
      return false;

    DCHECK_GE(bytes_left, kPsshSystemIdSize);
    if (!std::equal(kWidevineUuid,
                    kWidevineUuid + sizeof(kWidevineUuid), cur)) {
      cur = box_end;
      bytes_left = data_end - cur;
      continue;
    }

    cur += kPsshSystemIdSize;
    bytes_left -= kPsshSystemIdSize;

    // If KeyIDs specified, skip them.
    if (version > 0) {
      DCHECK_GE(bytes_left, kPsshKidCountSize);
      uint32_t kid_count = ReadUint32(cur);
      cur += kPsshKidCountSize + kid_count * kPsshKidSize;
      bytes_left -= kPsshKidCountSize + kid_count * kPsshKidSize;
      // Must be bytes left in this box for data_size.
      if (box_end < cur + kPsshDataSizeSize)
        return false;
    }

    DCHECK_GE(bytes_left, kPsshDataSizeSize);
    uint32_t data_size = ReadUint32(cur);
    cur += kPsshDataSizeSize;
    bytes_left -= kPsshDataSizeSize;

    if (box_end < cur + data_size)
      return false;

    pssh_data->assign(cur, cur + data_size);
    return true;
  }

  return false;
}

}

WidevineDrmDelegateAndroid::WidevineDrmDelegateAndroid() {
}

WidevineDrmDelegateAndroid::~WidevineDrmDelegateAndroid() {
}

const std::vector<uint8_t> WidevineDrmDelegateAndroid::GetUUID() const {
  return std::vector<uint8_t>(kWidevineUuid,
                              kWidevineUuid + arraysize(kWidevineUuid));
}

bool WidevineDrmDelegateAndroid::OnCreateSession(
    const media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::vector<uint8_t>* init_data_out,
    std::vector<std::string>* /* optional_parameters_out */) {
  if (init_data_type != media::EmeInitDataType::CENC)
    return true;

  // Widevine MediaDrm plugin only accepts the "data" part of the PSSH box as
  // the init data when using MP4 container.
  return GetPsshData(init_data, init_data_out);
}

}  // namespace cdm
