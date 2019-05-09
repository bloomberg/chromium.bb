// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_DEVICE_STATE_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_DEVICE_STATE_MIXIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"

namespace chromeos {

class DeviceStateMixin : public InProcessBrowserTestMixin {
 public:
  enum class State {
    BEFORE_OOBE,
    OOBE_COMPLETED_UNOWNED,
    OOBE_COMPLETED_CLOUD_ENROLLED,
    OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED,
    OOBE_COMPLETED_CONSUMER_OWNED,
    OOBE_COMPLETED_DEMO_MODE,
  };

  DeviceStateMixin(InProcessBrowserTestMixinHost* host, State initial_state);
  ~DeviceStateMixin() override;

  // InProcessBrowserTestMixin:
  bool SetUpUserDataDirectory() override;

  void SetState(State state);
  void set_domain(const std::string& domain) { domain_ = domain; }

 private:
  void SetDeviceState();
  void WriteInstallAttrFile();
  void WriteOwnerKey();

  State state_;
  std::string domain_;

  bool is_setup_ = false;

  DISALLOW_COPY_AND_ASSIGN(DeviceStateMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_DEVICE_STATE_MIXIN_H_
