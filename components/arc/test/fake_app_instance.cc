// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_app_instance.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "mojo/common/common_type_converters.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::AppInfoPtr, arc::mojom::AppInfo> {
  static arc::mojom::AppInfoPtr Convert(const arc::mojom::AppInfo& app_info) {
    return app_info.Clone();
  }
};

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace arc {

FakeAppInstance::FakeAppInstance(mojom::AppHost* app_host)
    : app_host_(app_host) {}
FakeAppInstance::~FakeAppInstance() {}

void FakeAppInstance::RefreshAppList() {
  ++refresh_app_list_count_;
}

void FakeAppInstance::LaunchApp(const std::string& package_name,
                                const std::string& activity,
                                const base::Optional<gfx::Rect>& dimension) {
  launch_requests_.push_back(base::MakeUnique<Request>(package_name, activity));
}

void FakeAppInstance::RequestAppIcon(const std::string& package_name,
                                     const std::string& activity,
                                     mojom::ScaleFactor scale_factor) {
  icon_requests_.push_back(
      base::MakeUnique<IconRequest>(package_name, activity, scale_factor));
}

void FakeAppInstance::SendRefreshAppList(
    const std::vector<mojom::AppInfo>& apps) {
  std::vector<mojom::AppInfoPtr> v;
  for (const auto& app : apps)
    v.emplace_back(app.Clone());
  app_host_->OnAppListRefreshed(std::move(v));
}

void FakeAppInstance::SendPackageAppListRefreshed(
    const std::string& package_name,
    const std::vector<mojom::AppInfo>& apps) {
  std::vector<mojom::AppInfoPtr> v;
  for (const auto& app : apps)
    v.emplace_back(app.Clone());
  app_host_->OnPackageAppListRefreshed(package_name, std::move(v));
}

void FakeAppInstance::SendInstallShortcuts(
    const std::vector<mojom::ShortcutInfo>& shortcuts) {
  for (auto& shortcut : shortcuts) {
    app_host_->OnInstallShortcut(shortcut.Clone());
  }
}

void FakeAppInstance::SendInstallShortcut(const mojom::ShortcutInfo& shortcut) {
  app_host_->OnInstallShortcut(shortcut.Clone());
}

void FakeAppInstance::SendAppAdded(const mojom::AppInfo& app) {
  app_host_->OnAppAddedDeprecated(mojom::AppInfo::From(app));
}

void FakeAppInstance::SendTaskCreated(int32_t taskId,
                                      const mojom::AppInfo& app,
                                      const std::string& intent) {
  app_host_->OnTaskCreated(taskId,
                           app.package_name,
                           app.activity,
                           app.name,
                           intent);
}

void FakeAppInstance::SendTaskDestroyed(int32_t taskId) {
  app_host_->OnTaskDestroyed(taskId);
}

bool FakeAppInstance::GenerateAndSendIcon(const mojom::AppInfo& app,
                                          mojom::ScaleFactor scale_factor,
                                          std::string* png_data_as_string) {
  if (!GetFakeIcon(scale_factor, png_data_as_string)) {
    return false;
  }

  app_host_->OnAppIcon(app.package_name, app.activity, scale_factor,
                       mojo::Array<uint8_t>::From(*png_data_as_string));

  return true;
}

bool FakeAppInstance::GetFakeIcon(mojom::ScaleFactor scale_factor,
                                  std::string* png_data_as_string) {
  CHECK(png_data_as_string != nullptr);
  std::string icon_file_name;
  switch (scale_factor) {
    case mojom::ScaleFactor::SCALE_FACTOR_100P:
      icon_file_name = "icon_100p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_125P:
      icon_file_name = "icon_125p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_133P:
      icon_file_name = "icon_133p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_140P:
      icon_file_name = "icon_140p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_150P:
      icon_file_name = "icon_150p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_180P:
      icon_file_name = "icon_180p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_200P:
      icon_file_name = "icon_200p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_250P:
      icon_file_name = "icon_250p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_300P:
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

  return true;
}

void FakeAppInstance::SetTaskInfo(int32_t task_id,
                                  const std::string& package_name,
                                  const std::string& activity) {
  task_id_to_info_[task_id].reset(new Request(package_name, activity));
}

void FakeAppInstance::SendRefreshPackageList(
    const std::vector<mojom::ArcPackageInfo>& packages) {
  std::vector<mojom::ArcPackageInfoPtr> v;
  for (const auto& package : packages)
    v.emplace_back(package.Clone());
  app_host_->OnPackageListRefreshed(std::move(v));
}

void FakeAppInstance::SendPackageAdded(const mojom::ArcPackageInfo& package) {
  app_host_->OnPackageAdded(mojom::ArcPackageInfoPtr(package.Clone()));
}

void FakeAppInstance::SendPackageUninstalled(const std::string& package_name) {
  app_host_->OnPackageRemoved(package_name);
}

void FakeAppInstance::CanHandleResolution(
    const std::string& package_name,
    const std::string& activity,
    const gfx::Rect& dimension,
    const CanHandleResolutionCallback& callback) {
  callback.Run(true);
}

void FakeAppInstance::UninstallPackage(const std::string& package_name) {
  app_host_->OnPackageRemoved(package_name);
}

void FakeAppInstance::GetTaskInfo(int32_t task_id,
                                  const GetTaskInfoCallback& callback) {
  TaskIdToInfo::const_iterator it = task_id_to_info_.find(task_id);
  if (it != task_id_to_info_.end())
    callback.Run(it->second->package_name(), it->second->activity());
  else
    callback.Run(std::string(), std::string());
}

void FakeAppInstance::SetTaskActive(int32_t task_id) {
}

void FakeAppInstance::CloseTask(int32_t task_id) {
}

void FakeAppInstance::ShowPackageInfoDeprecated(
    const std::string& package_name,
    const gfx::Rect& dimension_on_screen) {}

void FakeAppInstance::ShowPackageInfoOnPage(
    const std::string& package_name,
    mojom::ShowPackageInfoPage page,
    const gfx::Rect& dimension_on_screen) {}

void FakeAppInstance::SetNotificationsEnabled(const std::string& package_name,
                                              bool enabled) {}

void FakeAppInstance::InstallPackage(mojom::ArcPackageInfoPtr arcPackageInfo) {
  app_host_->OnPackageAdded(std::move(arcPackageInfo));
}

void FakeAppInstance::LaunchIntent(
    const std::string& intent_uri,
    const base::Optional<gfx::Rect>& dimension_on_screen) {
  launch_intents_.push_back(intent_uri);
}

void FakeAppInstance::RequestIcon(const std::string& icon_resource_id,
                                  arc::mojom::ScaleFactor scale_factor,
                                  const RequestIconCallback& callback) {
  shortcut_icon_requests_.push_back(
      base::MakeUnique<ShortcutIconRequest>(icon_resource_id, scale_factor));

  std::string png_data_as_string;
  if (GetFakeIcon(scale_factor, &png_data_as_string)) {
    callback.Run(mojo::Array<uint8_t>::From(png_data_as_string));
  }
}

void FakeAppInstance::RemoveCachedIcon(const std::string& icon_resource_id) {}

}  // namespace arc
