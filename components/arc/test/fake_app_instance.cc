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
#include "base/strings/stringprintf.h"

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

void FakeAppInstance::Init(mojom::AppHostPtr host_ptr) {
  // ARC app instance calls RefreshAppList after Init() successfully. Call
  // RefreshAppList() here to keep the same behavior.
  RefreshAppList();
}

void FakeAppInstance::RefreshAppList() {
  ++refresh_app_list_count_;
}

void FakeAppInstance::LaunchAppDeprecated(
    const std::string& package_name,
    const std::string& activity,
    const base::Optional<gfx::Rect>& dimension) {
  LaunchApp(package_name, activity, 0);
}

void FakeAppInstance::LaunchApp(const std::string& package_name,
                                const std::string& activity,
                                int64_t display_id) {
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
  for (auto& shortcut : shortcuts)
    SendInstallShortcut(shortcut);
}

void FakeAppInstance::SendInstallShortcut(const mojom::ShortcutInfo& shortcut) {
  app_host_->OnInstallShortcut(shortcut.Clone());
}

void FakeAppInstance::SendUninstallShortcut(const std::string& package_name,
                                            const std::string& intent_uri) {
  app_host_->OnUninstallShortcut(package_name, intent_uri);
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

void FakeAppInstance::SendTaskDescription(
    int32_t taskId,
    const std::string& label,
    const std::string& icon_png_data_as_string) {
  app_host_->OnTaskDescriptionUpdated(
      taskId, label,
      std::vector<uint8_t>(icon_png_data_as_string.begin(),
                           icon_png_data_as_string.end()));
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
                       std::vector<uint8_t>(png_data_as_string->begin(),
                                            png_data_as_string->end()));

  return true;
}

void FakeAppInstance::GenerateAndSendBadIcon(const mojom::AppInfo& app,
                                             mojom::ScaleFactor scale_factor) {
  std::vector<uint8_t> badIcon(10, 1);
  app_host_->OnAppIcon(app.package_name, app.activity, scale_factor, badIcon);
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

void FakeAppInstance::SendPackageModified(
    const mojom::ArcPackageInfo& package) {
  app_host_->OnPackageModified(mojom::ArcPackageInfoPtr(package.Clone()));
}

void FakeAppInstance::SendPackageUninstalled(const std::string& package_name) {
  app_host_->OnPackageRemoved(package_name);
}

void FakeAppInstance::SendInstallationStarted(const std::string& package_name) {
  app_host_->OnInstallationStarted(package_name);
}

void FakeAppInstance::SendInstallationFinished(const std::string& package_name,
                                               bool success) {
  mojom::InstallationResult result;
  result.package_name = package_name;
  result.success = success;
  app_host_->OnInstallationFinished(
      mojom::InstallationResultPtr(result.Clone()));
}

void FakeAppInstance::CanHandleResolutionDeprecated(
    const std::string& package_name,
    const std::string& activity,
    const gfx::Rect& dimension,
    const CanHandleResolutionDeprecatedCallback& callback) {
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

void FakeAppInstance::ShowPackageInfoOnPageDeprecated(
    const std::string& package_name,
    mojom::ShowPackageInfoPage page,
    const gfx::Rect& dimension_on_screen) {}

void FakeAppInstance::ShowPackageInfoOnPage(const std::string& package_name,
                                            mojom::ShowPackageInfoPage page,
                                            int64_t display_id) {}

void FakeAppInstance::SetNotificationsEnabled(const std::string& package_name,
                                              bool enabled) {}

void FakeAppInstance::InstallPackage(mojom::ArcPackageInfoPtr arcPackageInfo) {
  app_host_->OnPackageAdded(std::move(arcPackageInfo));
}

void FakeAppInstance::GetRecentAndSuggestedAppsFromPlayStore(
    const std::string& query,
    int32_t max_results,
    const GetRecentAndSuggestedAppsFromPlayStoreCallback& callback) {
  // Fake Play Store app info
  std::vector<arc::mojom::AppDiscoveryResultPtr> fake_apps;

  // Fake icon data.
  std::string png_data_as_string;
  GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P, &png_data_as_string);
  std::vector<uint8_t> fake_icon_png_data(png_data_as_string.begin(),
                                          png_data_as_string.end());

  fake_apps.push_back(mojom::AppDiscoveryResult::New(
      std::string("LauncherIntentUri"),        // launch_intent_uri
      std::string("InstallIntentUri"),         // install_intent_uri
      std::string(query),                      // label
      false,                                   // is_instant_app
      false,                                   // is_recent
      std::string("Publisher"),                // publisher_name
      std::string("$7.22"),                    // formatted_price
      5,                                       // review_score
      fake_icon_png_data,                      // icon_png_data
      std::string("com.google.android.gm")));  // package_name

  for (int i = 0; i < max_results - 1; ++i) {
    fake_apps.push_back(mojom::AppDiscoveryResult::New(
        base::StringPrintf("LauncherIntentUri %d", i),  // launch_intent_uri
        base::StringPrintf("InstallIntentUri %d", i),   // install_intent_uri
        base::StringPrintf("%s %d", query.c_str(), i),  // label
        i % 2 == 0,                                     // is_instant_app
        i % 4 == 0,                                     // is_recent
        base::StringPrintf("Publisher %d", i),          // publisher_name
        base::StringPrintf("$%d.22", i),                // formatted_price
        i,                                              // review_score
        fake_icon_png_data,                             // icon_png_data
        base::StringPrintf("test.package.%d", i)));     // package_name
  }
  callback.Run(arc::mojom::AppDiscoveryRequestState::SUCCESS,
               std::move(fake_apps));
}

void FakeAppInstance::StartPaiFlow() {
  ++start_pai_request_count_;
}

void FakeAppInstance::LaunchIntentDeprecated(
    const std::string& intent_uri,
    const base::Optional<gfx::Rect>& dimension_on_screen) {
  LaunchIntent(intent_uri, 0);
}

void FakeAppInstance::LaunchIntent(const std::string& intent_uri,
                                   int64_t display_id) {
  launch_intents_.push_back(intent_uri);
}

void FakeAppInstance::RequestIcon(const std::string& icon_resource_id,
                                  arc::mojom::ScaleFactor scale_factor,
                                  const RequestIconCallback& callback) {
  shortcut_icon_requests_.push_back(
      base::MakeUnique<ShortcutIconRequest>(icon_resource_id, scale_factor));

  std::string png_data_as_string;
  if (GetFakeIcon(scale_factor, &png_data_as_string)) {
    callback.Run(std::vector<uint8_t>(png_data_as_string.begin(),
                                      png_data_as_string.end()));
  }
}

void FakeAppInstance::RemoveCachedIcon(const std::string& icon_resource_id) {}

}  // namespace arc
