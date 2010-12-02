// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/device_management_policy_provider.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/profile_policy_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"

namespace policy {

ProfilePolicyContext::ProfilePolicyContext(Profile* profile)
    : profile_(profile) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl)) {
    device_management_service_.reset(new DeviceManagementService(
        command_line->GetSwitchValueASCII(switches::kDeviceManagementUrl)));
    device_management_policy_provider_.reset(
        new policy::DeviceManagementPolicyProvider(
            ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
            device_management_service_->CreateBackend(),
            profile_));
  }
}

ProfilePolicyContext::~ProfilePolicyContext() {
  device_management_policy_provider_.reset();
  device_management_service_.reset();
}

void ProfilePolicyContext::Initialize() {
  if (device_management_service_.get())
    device_management_service_->Initialize(profile_->GetRequestContext());
}

void ProfilePolicyContext::Shutdown() {
  if (device_management_service_.get())
    device_management_service_->Shutdown();
  if (device_management_policy_provider_.get())
    device_management_policy_provider_->Shutdown();
}

DeviceManagementPolicyProvider*
ProfilePolicyContext::GetDeviceManagementPolicyProvider() {
  return device_management_policy_provider_.get();
}

}  // namespace policy
