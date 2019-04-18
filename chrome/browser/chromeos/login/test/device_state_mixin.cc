// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/device_state_mixin.h"

#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_paths.h"
#include "components/policy/proto/install_attributes.pb.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr char kFakeDomain[] = "example.com";
constexpr char kFakeDeviceId[] = "device_id";

cryptohome::SerializedInstallAttributes BuildInstallAttributes(
    const std::string& mode,
    const std::string& domain,
    const std::string& realm,
    const std::string& device_id) {
  std::map<std::string, std::string> install_attrs_;
  install_attrs_["enterprise.mode"] = mode;
  install_attrs_["enterprise.domain"] = domain;
  install_attrs_["enterprise.realm"] = realm;
  install_attrs_["enterprise.device_id"] = device_id;
  install_attrs_["enterprise.owned"] = "true";

  cryptohome::SerializedInstallAttributes install_attrs;
  for (const auto& it : install_attrs_) {
    cryptohome::SerializedInstallAttributes::Attribute* attr_entry =
        install_attrs.add_attributes();
    const std::string& name = it.first;
    const std::string& value = it.second;
    attr_entry->set_name(name);
    attr_entry->mutable_value()->assign(value.data(),
                                        value.data() + value.size());
  }
  return install_attrs;
}

}  // namespace

DeviceStateMixin::DeviceStateMixin(InProcessBrowserTestMixinHost* host,
                                   State initial_state)
    : InProcessBrowserTestMixin(host), state_(initial_state) {}

bool DeviceStateMixin::SetUpUserDataDirectory() {
  SetDeviceState();
  return true;
}

void DeviceStateMixin::SetState(State state) {
  DCHECK(!is_setup_) << "SetState called after device was set up";
  state_ = state;
}

void DeviceStateMixin::SetDeviceState() {
  DCHECK(!is_setup_);
  DCHECK(domain_.empty() || state_ == State::OOBE_COMPLETED_CLOUD_ENROLLED);
  is_setup_ = true;

  WriteInstallAttrFile();
}

void DeviceStateMixin::WriteInstallAttrFile() {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::FilePath install_attrs_file =
      user_data_dir.Append("stub_install_attributes.pb");
  if (base::PathExists(install_attrs_file)) {
    return;
  }

  std::string device_mode, domain, realm, device_id;
  switch (state_) {
    case DeviceStateMixin::State::BEFORE_OOBE:
    case DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED:
      return;
    case DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED:
      device_mode = "enterprise";
      domain = !domain_.empty() ? domain_ : kFakeDomain;
      break;
    case DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED:
      device_mode = "enterprise_ad";
      realm = kFakeDomain;
      break;
    case DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED:
      device_mode = "consumer";
      break;
    case DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE:
      device_mode = "demo_mode";
      domain = "cros-demo-mode.com";
      break;
  }

  std::string blob;
  CHECK(BuildInstallAttributes(device_mode, domain, realm, kFakeDeviceId)
            .SerializeToString(&blob));
  CHECK_EQ(static_cast<int>(blob.size()),
           base::WriteFile(install_attrs_file, blob.data(), blob.size()));
}

DeviceStateMixin::~DeviceStateMixin() = default;

}  // namespace chromeos
