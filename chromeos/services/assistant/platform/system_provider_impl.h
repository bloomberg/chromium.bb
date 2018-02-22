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
  std::string GetDeviceModelId() override;
  int GetDeviceModelRevision() override;
  std::string GetEmbedderBuildInfo() override;
  std::string GetBoardName() override;
  std::string GetBoardRevision() override;
  std::string GetOemDeviceId() override;
  std::string GetDisplayName() override;
  int GetDebugServerPort() override;

 private:
  std::string board_name_;
  std::string device_model_id_;
  std::string embedder_build_info_;

  DISALLOW_COPY_AND_ASSIGN(SystemProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_SYSTEM_PROVIDER_IMPL_H_
