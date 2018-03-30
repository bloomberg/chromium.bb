// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_CORE_AUDIO_UTIL_MAC_H_
#define MEDIA_AUDIO_MAC_CORE_AUDIO_UTIL_MAC_H_

#include <CoreAudio/AudioHardware.h>

#include <string>
#include <vector>

#include "base/optional.h"

namespace media {
namespace core_audio_mac {

// Returns a vector with the IDs of all audio devices in the system.
// The vector is empty if there are no devices or if there is an error.
std::vector<AudioObjectID> GetAllAudioDeviceIDs();

// Returns a vector with the IDs of all devices related to the given
// |device_id|. The vector is empty if there are no related devices or
// if there is an error.
std::vector<AudioObjectID> GetRelatedDeviceIDs(AudioObjectID device_id);

// Returns a string with a unique device ID for the given |device_id|, or no
// value if there is an error.
base::Optional<std::string> GetDeviceUniqueID(AudioObjectID device_id);

// Returns a string with a descriptive label for the given |device_id|, or no
// value if there is an error. The returned label is based on several
// characteristics of the device.
base::Optional<std::string> GetDeviceLabel(AudioObjectID device_id,
                                           bool is_input);

// Returns the number of input or output streams associated with the given
// |device_id|. Returns zero if there are no streams or if there is an error.
uint32_t GetNumStreams(AudioObjectID device_id, bool is_input);

}  // namespace core_audio_mac
}  // namespace media

#endif  // MEDIA_AUDIO_MAC_CORE_AUDIO_UTIL_MAC_H_
