// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_FAKE_STATISTICS_PROVIDER_H_
#define CHROMEOS_SYSTEM_FAKE_STATISTICS_PROVIDER_H_

#include <map>
#include <string>

#include "chromeos/system/statistics_provider.h"

namespace chromeos {
namespace system {

// A fake StatisticsProvider implementation that is useful in tests.
class FakeStatisticsProvider : public StatisticsProvider {
 public:
  FakeStatisticsProvider();
  ~FakeStatisticsProvider() override;

  // StatisticsProvider implementation:
  void StartLoadingMachineStatistics(
      const scoped_refptr<base::TaskRunner>& file_task_runner,
      bool load_oem_manifest) override;
  bool GetMachineStatistic(const std::string& name,
                           std::string* result) override;
  bool HasMachineStatistic(const std::string& name) override;
  bool GetMachineFlag(const std::string& name, bool* result) override;
  bool HasMachineFlag(const std::string& name) override;
  void Shutdown() override;

  void SetMachineStatistic(const std::string& key, const std::string& value);
  void ClearMachineStatistic(const std::string& key);
  void SetMachineFlag(const std::string& key, bool value);
  void ClearMachineFlag(const std::string& key);

 private:
  std::map<std::string, std::string> machine_statistics_;
  std::map<std::string, bool> machine_flags_;

  DISALLOW_COPY_AND_ASSIGN(FakeStatisticsProvider);
};

// A convenience subclass that automatically registers itself as the test
// StatisticsProvider during construction and cleans up at destruction.
class ScopedFakeStatisticsProvider : public FakeStatisticsProvider {
 public:
  ScopedFakeStatisticsProvider();
  ~ScopedFakeStatisticsProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedFakeStatisticsProvider);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_FAKE_STATISTICS_PROVIDER_H_
