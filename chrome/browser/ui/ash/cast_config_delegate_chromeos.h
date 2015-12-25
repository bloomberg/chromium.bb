// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_CHROMEOS_H_

#include <string>

#include "ash/cast_config_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/values.h"

namespace chromeos {

// A class which allows the ash tray to communicate with the cast extension.
class CastConfigDelegateChromeos : public ash::CastConfigDelegate {
 public:
  CastConfigDelegateChromeos();
  ~CastConfigDelegateChromeos() override;

 private:
  // CastConfigDelegate:
  bool HasCastExtension() const override;
  DeviceUpdateSubscription RegisterDeviceUpdateObserver(
      const ReceiversAndActivitesCallback& callback) override;
  void RequestDeviceRefresh() override;
  void CastToReceiver(const std::string& receiver_id) override;
  void StopCasting(const std::string& activity_id) override;
  bool HasOptions() const override;
  void LaunchCastOptions() override;

  DISALLOW_COPY_AND_ASSIGN(CastConfigDelegateChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_CHROMEOS_H_
