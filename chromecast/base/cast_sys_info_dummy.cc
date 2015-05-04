// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_dummy.h"

namespace chromecast {

CastSysInfoDummy::CastSysInfoDummy() {
}

CastSysInfoDummy::~CastSysInfoDummy() {
}

CastSysInfo::BuildType CastSysInfoDummy::GetBuildType() {
  return BUILD_ENG;
}

std::string CastSysInfoDummy::GetSystemReleaseChannel() {
  return "";
}

std::string CastSysInfoDummy::GetSerialNumber() {
  return "dummy.serial.number";
}

std::string CastSysInfoDummy::GetProductName() {
  return "";
}

std::string CastSysInfoDummy::GetDeviceModel() {
  return "";
}

std::string CastSysInfoDummy::GetBoardName() {
  return "";
}

std::string CastSysInfoDummy::GetBoardRevision() {
  return "";
}

std::string CastSysInfoDummy::GetManufacturer() {
  return "";
}

std::string CastSysInfoDummy::GetSystemBuildNumber() {
  return "";
}

std::string CastSysInfoDummy::GetFactoryCountry() {
  return "US";
}

std::string CastSysInfoDummy::GetFactoryLocale(std::string* second_locale) {
  return "en-US";
}

std::string CastSysInfoDummy::GetWifiInterface() {
  return "";
}

std::string CastSysInfoDummy::GetApInterface() {
  return "";
}

std::string CastSysInfoDummy::GetGlVendor() {
  return "";
}

std::string CastSysInfoDummy::GetGlRenderer() {
  return "";
}

std::string CastSysInfoDummy::GetGlVersion() {
  return "";
}

}  // namespace chromecast
