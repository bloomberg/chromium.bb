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

SystemProviderImpl::SystemProviderImpl()
    : board_name_(base::SysInfo::GetLsbReleaseBoard()),
      device_model_id_(base::StringPrintf("cros-%s", board_name_.c_str())) {
  embedder_build_info_ = chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_FULL);
}

SystemProviderImpl::~SystemProviderImpl() = default;

std::string SystemProviderImpl::GetDeviceModelId() {
  return device_model_id_;
}

int SystemProviderImpl::GetDeviceModelRevision() {
  return 0;
}

std::string SystemProviderImpl::GetEmbedderBuildInfo() {
  return embedder_build_info_;
}

std::string SystemProviderImpl::GetBoardName() {
  return board_name_;
}

std::string SystemProviderImpl::GetBoardRevision() {
  return "0";
}

int SystemProviderImpl::GetDebugServerPort() {
#if DCHECK_IS_ON()
  return 8007;
#else   // DCHECK_IS_ON()
  // -1 disables debug server.
  return -1;
#endif  // DCHECK_IS_ON()
}

std::string SystemProviderImpl::GetOemDeviceId() {
  return board_name_;
}

std::string SystemProviderImpl::GetDisplayName() {
  return board_name_;
}

}  // namespace assistant
}  // namespace chromeos
