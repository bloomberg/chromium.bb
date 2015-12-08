// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace arc {

FakeArcBridgeService::FakeArcBridgeService() {
}

FakeArcBridgeService::~FakeArcBridgeService() {
  if (state() != State::STOPPED) {
    SetState(State::STOPPED);
  }
}

void FakeArcBridgeService::DetectAvailability() {
}

void FakeArcBridgeService::HandleStartup() {
}

void FakeArcBridgeService::Shutdown() {
}

bool FakeArcBridgeService::RegisterInputDevice(const std::string& name,
                                               const std::string& device_type,
                                               base::ScopedFD fd) {
  return true;
}

bool FakeArcBridgeService::SendNotificationEventToAndroid(
    const std::string& key,
    ArcNotificationEvent event) {
  return true;
}

bool FakeArcBridgeService::RefreshAppList() {
  ++refresh_app_list_count_;
  return true;
}

bool FakeArcBridgeService::LaunchApp(const std::string& package,
                                     const std::string& activity) {
  launch_requests_.push_back(new Request(package, activity));
  return true;
}

bool FakeArcBridgeService::RequestAppIcon(const std::string& package,
                                          const std::string& activity,
                                          ScaleFactor scale_factor) {
  icon_requests_.push_back(new IconRequest(package, activity, scale_factor));
  return true;
}

void FakeArcBridgeService::SetReady() {
  SetState(State::READY);
}

void FakeArcBridgeService::SetStopped() {
  SetState(State::STOPPED);
}

bool FakeArcBridgeService::HasObserver(const Observer* observer) {
  return observer_list().HasObserver(observer);
}

bool FakeArcBridgeService::HasAppObserver(const AppObserver* observer) {
  return app_observer_list().HasObserver(observer);
}

void FakeArcBridgeService::SendRefreshAppList(
    const std::vector<AppInfo>& apps) {
  FOR_EACH_OBSERVER(AppObserver, app_observer_list(), OnAppListRefreshed(apps));
}

bool FakeArcBridgeService::GenerateAndSendIcon(
    const AppInfo& app,
    ScaleFactor scale_factor,
    std::string* png_data_as_string) {
  CHECK(png_data_as_string != nullptr);
  std::string icon_file_name;
  switch (scale_factor) {
    case SCALE_FACTOR_SCALE_FACTOR_100P:
      icon_file_name = "icon_100p.png";
      break;
    case SCALE_FACTOR_SCALE_FACTOR_125P:
      icon_file_name = "icon_125p.png";
      break;
    case SCALE_FACTOR_SCALE_FACTOR_133P:
      icon_file_name = "icon_133p.png";
      break;
    case SCALE_FACTOR_SCALE_FACTOR_140P:
      icon_file_name = "icon_140p.png";
      break;
    case SCALE_FACTOR_SCALE_FACTOR_150P:
      icon_file_name = "icon_150p.png";
      break;
    case SCALE_FACTOR_SCALE_FACTOR_180P:
      icon_file_name = "icon_180p.png";
      break;
    case SCALE_FACTOR_SCALE_FACTOR_200P:
      icon_file_name = "icon_200p.png";
      break;
    case SCALE_FACTOR_SCALE_FACTOR_250P:
      icon_file_name = "icon_250p.png";
      break;
    case SCALE_FACTOR_SCALE_FACTOR_300P:
      icon_file_name = "icon_300p.png";
      break;
    default:
      NOTREACHED();
      return false;
  }

  base::FilePath base_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &base_path));
  base::FilePath icon_file_path = base_path
      .AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("arc")
      .AppendASCII(icon_file_name);
  CHECK(base::PathExists(icon_file_path));
  CHECK(base::ReadFileToString(icon_file_path, png_data_as_string));

  std::vector<uint8_t> png_data(png_data_as_string->begin(),
                                png_data_as_string->end());

  FOR_EACH_OBSERVER(AppObserver,
                    app_observer_list(),
                    OnAppIcon(app.package,
                              app.activity,
                              scale_factor,
                              png_data));

  return true;
}

}  // namespace arc
