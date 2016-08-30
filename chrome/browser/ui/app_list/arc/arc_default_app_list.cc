// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"

#include "base/files/file_enumerator.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/task_runner_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kActivity[] = "activity";
const char kAppPath[] = "app_path";
const char kName[] = "name";
const char kOem[] = "oem";
const char kPackageName[] = "package_name";

// Sub-directory wher Arc apps forward declarations are stored.
const base::FilePath::CharType kArcDirectory[] = FILE_PATH_LITERAL("arc");
const base::FilePath::CharType kArcTestDirectory[] =
    FILE_PATH_LITERAL("arc_default_apps");

bool use_test_apps_directory = false;

std::unique_ptr<ArcDefaultAppList::AppInfoMap>
ReadAppsFromFileThread() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  std::unique_ptr<ArcDefaultAppList::AppInfoMap> apps(
      new ArcDefaultAppList::AppInfoMap);

  base::FilePath base_path;
  if (!use_test_apps_directory) {
    if (!base::PathService::Get(chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                                &base_path))
      return apps;
    base_path = base_path.Append(kArcDirectory);
  } else {
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &base_path))
      return apps;
    base_path = base_path.AppendASCII(kArcTestDirectory);
  }

  base::FilePath::StringType extension(".json");
  base::FileEnumerator json_files(
      base_path,
      false,  // Recursive.
      base::FileEnumerator::FILES);

  for (base::FilePath file = json_files.Next(); !file.empty();
      file = json_files.Next()) {

    if (file.MatchesExtension(extension)) {
      JSONFileValueDeserializer deserializer(file);
      std::string error_msg;
      std::unique_ptr<base::Value> app_info =
          deserializer.Deserialize(nullptr, &error_msg);
      if (!app_info) {
        VLOG(2) << "Unable to deserialize json data: " << error_msg
                << " in file " << file.value() << ".";
        continue;
      }

      std::unique_ptr<base::DictionaryValue> app_info_dictionary =
          base::DictionaryValue::From(std::move(app_info));
      CHECK(app_info_dictionary);

      std::string name;
      std::string package_name;
      std::string activity;
      std::string app_path;
      bool oem = false;

      app_info_dictionary->GetString(kName, &name);
      app_info_dictionary->GetString(kPackageName, &package_name);
      app_info_dictionary->GetString(kActivity, &activity);
      app_info_dictionary->GetString(kAppPath, &app_path);
      app_info_dictionary->GetBoolean(kOem, &oem);

      if (name.empty() ||
          package_name.empty() ||
          activity.empty() ||
          app_path.empty()) {
        VLOG(2) << "Arc app declaration is incomplete in file "
                << file.value() << ".";
        continue;
      }

      const std::string app_id = ArcAppListPrefs::GetAppId(
          package_name, activity);
      std::unique_ptr<ArcDefaultAppList::AppInfo> app(
          new ArcDefaultAppList::AppInfo(name,
                                         package_name,
                                         activity,
                                         oem,
                                         base_path.Append(app_path)));
      apps.get()->insert(
          std::pair<std::string,
                    std::unique_ptr<ArcDefaultAppList::AppInfo>>(
                        app_id, std::move(app)));
    } else {
      DVLOG(1) << "Not considering: " << file.LossyDisplayName()
               << " (does not have a .json extension)";
    }
  }

  return apps;
}

}  // namespace

// static
void ArcDefaultAppList::UseTestAppsDirectory() {
  use_test_apps_directory = true;
}

ArcDefaultAppList::ArcDefaultAppList(Delegate* delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {
  CHECK(delegate_);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Once ready OnAppsReady is called.
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&ReadAppsFromFileThread),
      base::Bind(&ArcDefaultAppList::OnAppsReady,
          weak_ptr_factory_.GetWeakPtr()));
}

ArcDefaultAppList::~ArcDefaultAppList() {}

void ArcDefaultAppList::OnAppsReady(std::unique_ptr<AppInfoMap> apps) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  apps_.swap(*apps.get());

  // Initially consider packages are installed.
  for (const auto& app : apps_)
    packages_[app.second->package_name] = false;

  delegate_->OnDefaultAppsReady();
}

const ArcDefaultAppList::AppInfo* ArcDefaultAppList::GetApp(
    const std::string& app_id) const {
  const auto it = apps_.find(app_id);
  if (it == apps_.end())
    return nullptr;
  // Check if its package was uninstalled.
  const auto it_package = packages_.find(it->second->package_name);
  DCHECK(it_package != packages_.end());
  if (it_package->second)
    return nullptr;
  return it->second.get();
}

bool ArcDefaultAppList::HasApp(const std::string& app_id) const {
  return GetApp(app_id) != nullptr;
}

bool ArcDefaultAppList::HasPackage(const std::string& package_name) const {
  return packages_.count(package_name);
}

void ArcDefaultAppList::MaybeMarkPackageUninstalled(
    const std::string& package_name, bool uninstalled) {
  auto it = packages_.find(package_name);
  if (it == packages_.end())
    return;
  it->second = uninstalled;
}

ArcDefaultAppList::AppInfo::AppInfo(const std::string& name,
                                    const std::string& package_name,
                                    const std::string& activity,
                                    bool oem,
                                    const base::FilePath app_path)
    : name(name),
      package_name(package_name),
      activity(activity),
      oem(oem),
      app_path(app_path) {}

ArcDefaultAppList::AppInfo::~AppInfo() {}
