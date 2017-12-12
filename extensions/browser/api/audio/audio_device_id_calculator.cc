// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_device_id_calculator.h"

#include "base/strings/string_number_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/api/audio/pref_names.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

AudioDeviceIdCalculator::AudioDeviceIdCalculator(
    content::BrowserContext* context)
    : context_(context) {}

AudioDeviceIdCalculator::~AudioDeviceIdCalculator() {}

std::string AudioDeviceIdCalculator::GetStableDeviceId(
    uint64_t audio_service_stable_id) {
  if (!stable_id_map_loaded_)
    LoadStableIdMap();
  std::string audio_service_stable_id_str =
      base::NumberToString(audio_service_stable_id);
  const auto& it = stable_id_map_.find(audio_service_stable_id_str);
  if (it != stable_id_map_.end())
    return it->second;
  return GenerateNewStableDeviceId(audio_service_stable_id_str);
}

void AudioDeviceIdCalculator::LoadStableIdMap() {
  DCHECK(!stable_id_map_loaded_);

  PrefService* pref_service =
      ExtensionsBrowserClient::Get()->GetPrefServiceForContext(context_);
  const base::ListValue* audio_service_stable_ids =
      pref_service->GetList(kAudioApiStableDeviceIds);
  for (size_t i = 0; i < audio_service_stable_ids->GetSize(); ++i) {
    std::string audio_service_stable_id;
    if (!audio_service_stable_ids->GetString(i, &audio_service_stable_id)) {
      NOTREACHED() << "Non string stable device ID.";
      continue;
    }
    stable_id_map_[audio_service_stable_id] = base::NumberToString(i);
  }
  stable_id_map_loaded_ = true;
}

std::string AudioDeviceIdCalculator::GenerateNewStableDeviceId(
    const std::string& audio_service_stable_id) {
  DCHECK(stable_id_map_loaded_);
  DCHECK_EQ(0u, stable_id_map_.count(audio_service_stable_id));

  ListPrefUpdate update(
      ExtensionsBrowserClient::Get()->GetPrefServiceForContext(context_),
      kAudioApiStableDeviceIds);

  std::string api_stable_id = base::NumberToString(update.Get()->GetSize());
  stable_id_map_[audio_service_stable_id] = api_stable_id;
  update->AppendString(audio_service_stable_id);
  return api_stable_id;
}

}  // namespace extensions
