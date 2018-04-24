// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/system_provider_impl.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "chromeos/system/version_loader.h"

namespace chromeos {
namespace assistant {

SystemProviderImpl::SystemProviderImpl() = default;

SystemProviderImpl::~SystemProviderImpl() = default;

assistant_client::MicMuteState SystemProviderImpl::GetMicMuteState() {
  // TODO(xiaohuic): implement mic mute state.
  return assistant_client::MicMuteState::MICROPHONE_ENABLED;
}

void SystemProviderImpl::RegisterMicMuteChangeCallback(
    ConfigChangeCallback callback) {
  // TODO(xiaohuic): implement mic mute state.
}

assistant_client::PowerManagerProvider*
SystemProviderImpl::GetPowerManagerProvider() {
  // TODO(xiaohuic): implement power manager provider
  return nullptr;
}

bool SystemProviderImpl::GetBatteryState(BatteryState* state) {
  // TODO(xiaohuic): implement battery state
  return false;
}

}  // namespace assistant
}  // namespace chromeos
