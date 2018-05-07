// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_SYSTEM_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_SYSTEM_PROVIDER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "libassistant/shared/public/platform_system.h"

namespace chromeos {
namespace assistant {

class SystemProviderImpl : public assistant_client::SystemProvider {
 public:
  SystemProviderImpl();
  ~SystemProviderImpl() override;

  // assistant_client::SystemProvider implementation:
  assistant_client::MicMuteState GetMicMuteState() override;
  void RegisterMicMuteChangeCallback(ConfigChangeCallback callback) override;
  assistant_client::PowerManagerProvider* GetPowerManagerProvider() override;
  bool GetBatteryState(BatteryState* state) override;
  void UpdateTimezoneAndLocale(const std::string& timezone,
                               const std::string& locale) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_SYSTEM_PROVIDER_IMPL_H_
