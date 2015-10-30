// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CAST_SYS_INFO_DUMMY_H_
#define CHROMECAST_BASE_CAST_SYS_INFO_DUMMY_H_

// Note(slan): This file is needed by internal targets which cannot depend on
// "//base". Amend this include with a comment so gn check ignores it.
#include "base/macros.h"  // nogncheck
#include "chromecast/public/cast_sys_info.h"

namespace chromecast {

class CastSysInfoDummy : public CastSysInfo {
 public:
  CastSysInfoDummy();
  ~CastSysInfoDummy() override;

  // CastSysInfo implementation:
  BuildType GetBuildType() override;
  std::string GetSystemReleaseChannel() override;
  std::string GetSerialNumber() override;
  std::string GetProductName() override;
  std::string GetDeviceModel() override;
  std::string GetBoardName() override;
  std::string GetBoardRevision() override;
  std::string GetManufacturer() override;
  std::string GetSystemBuildNumber() override;
  std::string GetFactoryCountry() override;
  std::string GetFactoryLocale(std::string* second_locale) override;
  std::string GetWifiInterface() override;
  std::string GetApInterface() override;
  std::string GetGlVendor() override;
  std::string GetGlRenderer() override;
  std::string GetGlVersion() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastSysInfoDummy);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CAST_SYS_INFO_DUMMY_H_
