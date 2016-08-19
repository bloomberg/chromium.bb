// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/fake_statistics_provider.h"

namespace chromeos {
namespace system {

FakeStatisticsProvider::FakeStatisticsProvider() {
}

FakeStatisticsProvider::~FakeStatisticsProvider() {
}

void FakeStatisticsProvider::StartLoadingMachineStatistics(
    const scoped_refptr<base::TaskRunner>& file_task_runner,
    bool load_oem_manifest) {
}

bool FakeStatisticsProvider::GetMachineStatistic(const std::string& name,
                                                 std::string* result) {
  std::map<std::string, std::string>::const_iterator match =
      machine_statistics_.find(name);
  if (match != machine_statistics_.end() && result)
    *result = match->second;
  return match != machine_statistics_.end();
}

bool FakeStatisticsProvider::GetMachineFlag(const std::string& name,
                                            bool* result) {
  std::map<std::string, bool>::const_iterator match = machine_flags_.find(name);
  if (match != machine_flags_.end() && result)
    *result = match->second;
  return match != machine_flags_.end();
}

void FakeStatisticsProvider::Shutdown() {
}

bool FakeStatisticsProvider::IsRunningOnVm() {
  std::string is_vm;
  return GetMachineStatistic(kIsVmKey, &is_vm) && is_vm == kIsVmValueTrue;
}


void FakeStatisticsProvider::SetMachineStatistic(const std::string& key,
                                                 const std::string& value) {
  machine_statistics_[key] = value;
}

void FakeStatisticsProvider::ClearMachineStatistic(const std::string& key) {
  machine_statistics_.erase(key);
}

void FakeStatisticsProvider::SetMachineFlag(const std::string& key,
                                            bool value) {
  machine_flags_[key] = value;
}

void FakeStatisticsProvider::ClearMachineFlag(const std::string& key) {
  machine_flags_.erase(key);
}

ScopedFakeStatisticsProvider::ScopedFakeStatisticsProvider() {
  StatisticsProvider::SetTestProvider(this);
}

ScopedFakeStatisticsProvider::~ScopedFakeStatisticsProvider() {
  StatisticsProvider::SetTestProvider(NULL);
}

}  // namespace system
}  // namespace chromeos
