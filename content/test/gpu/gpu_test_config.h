// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_GPU_GPU_TEST_CONFIG_H_
#define CONTENT_TEST_GPU_GPU_TEST_CONFIG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace content {
struct GPUInfo;
}  // namespace content

class GPUTestConfig {
 public:
  enum OS {
    kOsUnknown = 0,
    kOsWinXP = 1 << 0,
    kOsWinVista = 1 << 1,
    kOsWin7 = 1 << 2,
    kOsWin8 = 1 << 3,
    kOsWin = kOsWinXP | kOsWinVista | kOsWin7 | kOsWin8,
    kOsMacLeopard = 1 << 4,
    kOsMacSnowLeopard = 1 << 5,
    kOsMacLion = 1 << 6,
    kOsMacMountainLion = 1 << 7,
    kOsMac = kOsMacLeopard | kOsMacSnowLeopard | kOsMacLion |
             kOsMacMountainLion,
    kOsLinux = 1 << 8,
    kOsChromeOS = 1 << 9,
    kOsAndroid = 1 << 10,
  };

  enum BuildType {
    kBuildTypeUnknown = 0,
    kBuildTypeRelease = 1 << 0,
    kBuildTypeDebug = 1 << 1,
  };

  GPUTestConfig();
  virtual ~GPUTestConfig();

  void set_os(int32 os);
  void set_gpu_device_id(uint32 id);
  void set_build_type(int32 build_type);

  virtual void AddGPUVendor(uint32 gpu_vendor);

  int32 os() const { return os_; }
  const std::vector<uint32>& gpu_vendor() const { return gpu_vendor_; }
  uint32 gpu_device_id() const { return gpu_device_id_; }
  int32 build_type() const { return build_type_; }

  // Check if the config is valid. For example, if gpu_device_id_ is set, but
  // gpu_vendor_ is unknown, then it's invalid.
  virtual bool IsValid() const;

  // Check if two configs overlap, i.e., if there exists a config that matches
  // both configs.
  bool OverlapsWith(const GPUTestConfig& config) const;

 protected:
  void ClearGPUVendor();

 private:
  // operating system.
  int32 os_;

  // GPU vendor.
  std::vector<uint32> gpu_vendor_;

  // GPU device id (unique to each vendor).
  uint32 gpu_device_id_;

  // Release or Debug.
  int32 build_type_;
};

class GPUTestBotConfig : public GPUTestConfig {
 public:
  GPUTestBotConfig() { }
  virtual ~GPUTestBotConfig();

  // This should only be called when no gpu_vendor is added.
  virtual void AddGPUVendor(uint32 gpu_vendor) OVERRIDE;

  // Return false if gpu_info does not have valid vendor_id and device_id.
  bool SetGPUInfo(const content::GPUInfo& gpu_info);

  // Check if the bot config is valid, i.e., if it is one valid test-bot
  // environment. For example, if a field is unknown, or if OS is not one
  // fully defined OS, then it's valid.
  virtual bool IsValid() const OVERRIDE;

  // Check if a bot config matches a test config, i.e., the test config is a
  // superset of the bot config.
  bool Matches(const GPUTestConfig& config) const;
  bool Matches(const std::string& config_data) const;

  // Setup the config with the current gpu testing environment.
  // If gpu_info is NULL, collect GPUInfo first.
  bool LoadCurrentConfig(const content::GPUInfo* gpu_info);

  // Check if this bot's config matches |config_data| or any of the |configs|.
  static bool CurrentConfigMatches(const std::string& config_data);
  static bool CurrentConfigMatches(const std::vector<std::string>& configs);
};

#endif  // CONTENT_TEST_GPU_GPU_TEST_CONFIG_H_

