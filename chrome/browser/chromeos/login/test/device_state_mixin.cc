// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/device_state_mixin.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr char kFakeDomain[] = "example.com";
constexpr char kFakeDeviceId[] = "device_id";

}  // namespace

DeviceStateMixin::DeviceStateMixin(InProcessBrowserTestMixinHost* host,
                                   State initial_state)
    : InProcessBrowserTestMixin(host), state_(initial_state) {}

void DeviceStateMixin::SetUpInProcessBrowserTestFixture() {
  SetDeviceState();
}

void DeviceStateMixin::SetState(State state) {
  DCHECK(!is_setup_) << "SetState called after device was set up";
  state_ = state;
}

void DeviceStateMixin::SetDeviceState() {
  DCHECK(!is_setup_);
  is_setup_ = true;
  switch (state_) {
    case State::BEFORE_OOBE:
    case State::OOBE_COMPLETED_UNOWNED:
      install_attributes_.Get()->Clear();
      return;
    case State::OOBE_COMPLETED_CLOUD_ENROLLED:
      install_attributes_.Get()->SetCloudManaged(kFakeDomain, kFakeDeviceId);
      return;
    case State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED:
      install_attributes_.Get()->SetActiveDirectoryManaged(kFakeDomain,
                                                           kFakeDeviceId);
      return;
    case State::OOBE_COMPLETED_CONSUMER_OWNED:
      install_attributes_.Get()->SetConsumerOwned();
      return;
  }
}

DeviceStateMixin::~DeviceStateMixin() = default;

}  // namespace chromeos
