// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_app_instance.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "mojo/common/common_type_converters.h"

namespace mojo {

template <>
struct TypeConverter<arc::AppInfoPtr, arc::AppInfo> {
  static arc::AppInfoPtr Convert(const arc::AppInfo& app_info) {
    return app_info.Clone();
  }
};

}  // namespace mojo

namespace arc {

FakeAppInstance::FakeAppInstance(AppHost* app_host)
    : binding_(this), app_host_(app_host) {}
FakeAppInstance::~FakeAppInstance() {}

void FakeAppInstance::RefreshAppList() {
  ++refresh_app_list_count_;
}

void FakeAppInstance::LaunchApp(const mojo::String& package_name,
                                const mojo::String& activity) {
  launch_requests_.push_back(new Request(package_name, activity));
}

void FakeAppInstance::RequestAppIcon(const mojo::String& package_name,
                                     const mojo::String& activity,
                                     ScaleFactor scale_factor) {
  icon_requests_.push_back(
      new IconRequest(package_name, activity, scale_factor));
}

void FakeAppInstance::SendRefreshAppList(const std::vector<AppInfo>& apps) {
  app_host_->OnAppListRefreshed(mojo::Array<AppInfoPtr>::From(apps));
}

bool FakeAppInstance::GenerateAndSendIcon(const AppInfo& app,
                                          ScaleFactor scale_factor,
                                          std::string* png_data_as_string) {
  CHECK(png_data_as_string != nullptr);
  std::string icon_file_name;
  switch (scale_factor) {
    case ScaleFactor::SCALE_FACTOR_100P:
      icon_file_name = "icon_100p.png";
      break;
    case ScaleFactor::SCALE_FACTOR_125P:
      icon_file_name = "icon_125p.png";
      break;
    case ScaleFactor::SCALE_FACTOR_133P:
      icon_file_name = "icon_133p.png";
      break;
    case ScaleFactor::SCALE_FACTOR_140P:
      icon_file_name = "icon_140p.png";
      break;
    case ScaleFactor::SCALE_FACTOR_150P:
      icon_file_name = "icon_150p.png";
      break;
    case ScaleFactor::SCALE_FACTOR_180P:
      icon_file_name = "icon_180p.png";
      break;
    case ScaleFactor::SCALE_FACTOR_200P:
      icon_file_name = "icon_200p.png";
      break;
    case ScaleFactor::SCALE_FACTOR_250P:
      icon_file_name = "icon_250p.png";
      break;
    case ScaleFactor::SCALE_FACTOR_300P:
      icon_file_name = "icon_300p.png";
      break;
    default:
      NOTREACHED();
      return false;
  }

  base::FilePath base_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &base_path));
  base::FilePath icon_file_path = base_path.AppendASCII("components")
                                      .AppendASCII("test")
                                      .AppendASCII("data")
                                      .AppendASCII("arc")
                                      .AppendASCII(icon_file_name);
  CHECK(base::PathExists(icon_file_path));
  CHECK(base::ReadFileToString(icon_file_path, png_data_as_string));

  app_host_->OnAppIcon(app.package_name, app.activity, scale_factor,
                       mojo::Array<uint8_t>::From(*png_data_as_string));

  return true;
}

void FakeAppInstance::WaitForIncomingMethodCall() {
  binding_.WaitForIncomingMethodCall();
}

void FakeAppInstance::WaitForOnAppInstanceReady() {
  // Several messages are sent back and forth when OnAppInstanceReady() is
  // called. Normally, it would be preferred to use a single
  // WaitForIncomingMethodCall() to wait for each method individually, but
  // QueryVersion() does require processing on the I/O thread, so
  // RunUntilIdle() is required to correctly dispatch it. On slower machines
  // (and when running under Valgrind), the two thread hops needed to send and
  // dispatch each Mojo message might not be picked up by a single
  // RunUntilIdle(), so keep pumping the message loop until all expected
  // messages are.
  while (refresh_app_list_count_ != 1) {
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace arc
