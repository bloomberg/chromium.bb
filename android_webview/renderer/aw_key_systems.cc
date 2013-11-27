// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_key_systems.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

using content::KeySystemInfo;

namespace {

const char kAudioMp4[] = "audio/mp4";
const char kVideoMp4[] = "video/mp4";
const char kMp4a[] = "mp4a";
const char kMp4aAvc1Avc3[] = "mp4a,avc1,avc3";

const uint8 kWidevineUuid[16] = {
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };

// Return |name|'s parent key system.
std::string GetDirectParentName(const std::string& name) {
  int last_period = name.find_last_of('.');
  DCHECK_GT(last_period, 0);
  return name.substr(0, last_period);
}

void AddWidevineWithCodecs(
    const std::string& key_system_name,
    bool add_parent_name,
    std::vector<KeySystemInfo>* concrete_key_systems) {
  KeySystemInfo info(key_system_name);

  if (add_parent_name)
    info.parent_key_system = GetDirectParentName(key_system_name);

  info.supported_types.push_back(std::make_pair(kAudioMp4, kMp4a));
  info.supported_types.push_back(std::make_pair(kVideoMp4, kMp4aAvc1Avc3));

  info.uuid.assign(kWidevineUuid, kWidevineUuid + arraysize(kWidevineUuid));

  concrete_key_systems->push_back(info);
}

}  // namespace

namespace android_webview {

void AwAddKeySystems(
    std::vector<KeySystemInfo>* key_systems_info) {
  AddWidevineWithCodecs(kWidevineKeySystem, true, key_systems_info);
}

}  // namespace android_webview
