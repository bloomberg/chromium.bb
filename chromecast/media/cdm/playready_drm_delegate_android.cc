// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/playready_drm_delegate_android.h"

#include "base/logging.h"
#include "media/base/bit_reader.h"

namespace chromecast {
namespace media {

const uint8_t kPlayreadyUuid[16] = {
    0x9a, 0x04, 0xf0, 0x79, 0x98, 0x40, 0x42, 0x86,
    0xab, 0x92, 0xe6, 0x5b, 0xe0, 0x88, 0x5f, 0x95};

const uint8_t kPlayreadyCustomDataUuid[] = {
    0x2b, 0xf8, 0x66, 0x80, 0xc6, 0xe5, 0x4e, 0x24,
    0xbe, 0x23, 0x0f, 0x81, 0x5a, 0x60, 0x6e, 0xb2};

// ASCII "uuid" as an 4-byte integer
const uint32_t kBoxTypeUuid = 1970628964;

PlayreadyDrmDelegateAndroid::PlayreadyDrmDelegateAndroid() {
}

PlayreadyDrmDelegateAndroid::~PlayreadyDrmDelegateAndroid() {
}

const ::media::UUID PlayreadyDrmDelegateAndroid::GetUUID() const {
  return ::media::UUID(kPlayreadyUuid,
                       kPlayreadyUuid + arraysize(kPlayreadyUuid));
}

bool PlayreadyDrmDelegateAndroid::OnCreateSession(
    const ::media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::vector<uint8_t>* /* init_data_out */,
      std::vector<std::string>* optional_parameters_out) {
  if (init_data_type == ::media::EmeInitDataType::CENC) {
    ::media::BitReader reader(&init_data[0], init_data.size());
    while (reader.bits_available() > 64) {
      uint32_t box_size;
      uint32_t box_type;
      reader.ReadBits(32, &box_size);
      reader.ReadBits(32, &box_type);
      int bytes_read = 8;

      if (box_type != kBoxTypeUuid) {
        if (box_size < 8 + sizeof(kPlayreadyCustomDataUuid)) {
          break;
        }
        // Box size includes the bytes already consumed
        reader.SkipBits((box_size - bytes_read) * 8);
        continue;
      }

      // "uuid" was found, look for custom data format as per b/10246367
      reader.SkipBits(128);
      bytes_read += 16;
      if (!memcmp(&init_data[0] + reader.bits_read() / 8,
                  kPlayreadyCustomDataUuid, 16)) {
        reader.SkipBits((box_size - bytes_read) * 8);
        continue;
      }

      int custom_data_size = box_size - bytes_read;
      DCHECK(reader.bits_read() % 8 == 0);
      int total_bytes_read = reader.bits_read() / 8;

      optional_parameters_out->clear();
      optional_parameters_out->push_back("PRCustomData");
      optional_parameters_out->push_back(
          std::string(&init_data[0] + total_bytes_read,
                      &init_data[0] + total_bytes_read + custom_data_size));
      reader.SkipBits(custom_data_size * 8);
      LOG(INFO) << "Including " << custom_data_size
                << " bytes of custom PlayReady data";
    }
  }
  return true;
}

}  // namespace media
}  // namespace chromecast
