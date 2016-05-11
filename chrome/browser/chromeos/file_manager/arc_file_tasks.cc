// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/arc_file_tasks.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/entry_info.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/filename_util.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

namespace {

constexpr int kArcInstanceHelperVersionWithUrlListSupport = 4;
constexpr base::FilePath::CharType kArcDownloadPath[] =
    FILE_PATH_LITERAL("/sdcard/Download");

// Returns the Mojo interface for ARC Intent Helper, with version |minVersion|
// or above. If the ARC bridge is not established, returns null.
arc::mojom::IntentHelperInstance* GetArcIntentHelper(Profile* profile,
                                                     int min_version) {
  // File manager in secondary profile cannot access ARC.
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return nullptr;

  arc::ArcBridgeService* arc_service = arc::ArcBridgeService::Get();
  if (!arc_service)
    return nullptr;

  arc::mojom::IntentHelperInstance* intent_helper_instance =
      arc_service->intent_helper_instance();
  if (!intent_helper_instance)
    return nullptr;

  if (arc_service->intent_helper_version() < min_version) {
    DLOG(WARNING) << "ARC intent helper instance is too old.";
    return nullptr;
  }
  return intent_helper_instance;
}

std::string ArcActionToString(arc::mojom::ActionType action) {
  switch (action) {
    case arc::mojom::ActionType::VIEW:
      return "view";
    case arc::mojom::ActionType::SEND:
      return "send";
    case arc::mojom::ActionType::SEND_MULTIPLE:
      return "send_multiple";
  }
  NOTREACHED();
  return "";
}

arc::mojom::ActionType StringToArcAction(const std::string& str) {
  if (str == "view")
    return arc::mojom::ActionType::VIEW;
  if (str == "send")
    return arc::mojom::ActionType::SEND;
  if (str == "send_multiple")
    return arc::mojom::ActionType::SEND_MULTIPLE;
  NOTREACHED();
  return arc::mojom::ActionType::VIEW;
}

// Converts the Chrome OS file path to ARC file URL.
bool ConvertToArcUrl(const base::FilePath& path, GURL* arc_url) {
  // Obtain the primary profile. This information is required because currently
  // only the file systems for the primary profile is exposed to ARC.
  if (!user_manager::UserManager::IsInitialized())
    return false;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return false;
  Profile* primary_profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (!primary_profile)
    return false;

  // Convert paths under primary profile's Downloads directory.
  base::FilePath primary_downloads =
      util::GetDownloadsFolderForProfile(primary_profile);
  base::FilePath result_path(kArcDownloadPath);
  if (primary_downloads.AppendRelativePath(path, &result_path)) {
    *arc_url = net::FilePathToFileURL(result_path);
    return true;
  }

  // TODO(kinaba): Add conversion logic once other file systems are supported.
  return false;
}

void OnArcHandlerList(
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    const FindTasksCallback& callback,
    mojo::Array<arc::mojom::UrlHandlerInfoPtr> handlers) {
  for (const arc::mojom::UrlHandlerInfoPtr& handler : handlers) {
    // TODO(crbug.com/578725): Wire action to "verb" once it's implemented.
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(handler->package_name, TASK_TYPE_ARC_APP,
                       ArcActionToString(handler->action)),
        handler->name,
        GURL(""),                                        // TODO: get the icon
        false,                                           // is_default,
        handler->action != arc::mojom::ActionType::VIEW  // is_generic
        ));
  }
  callback.Run(std::move(result_list));
}

}  // namespace

void FindArcTasks(Profile* profile,
                  const std::vector<extensions::EntryInfo>& entries,
                  std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
                  const FindTasksCallback& callback) {
  arc::mojom::IntentHelperInstance* arc_intent_helper =
      GetArcIntentHelper(profile, kArcInstanceHelperVersionWithUrlListSupport);
  if (!arc_intent_helper) {
    callback.Run(std::move(result_list));
    return;
  }

  mojo::Array<arc::mojom::UrlWithMimeTypePtr> urls;
  for (const extensions::EntryInfo& entry : entries) {
    GURL url;
    if (!ConvertToArcUrl(entry.path, &url)) {
      callback.Run(std::move(result_list));
      return;
    }

    arc::mojom::UrlWithMimeTypePtr url_with_type =
        arc::mojom::UrlWithMimeType::New();
    url_with_type->url = url.spec();
    url_with_type->mime_type = entry.mime_type;
    urls.push_back(std::move(url_with_type));
  }
  arc_intent_helper->RequestUrlListHandlerList(
      std::move(urls),
      base::Bind(&OnArcHandlerList, base::Passed(&result_list), callback));
}

bool ExecuteArcTask(Profile* profile,
                    const TaskDescriptor& task,
                    const std::vector<storage::FileSystemURL>& file_urls,
                    const std::vector<std::string>& mime_types) {
  DCHECK_EQ(file_urls.size(), mime_types.size());
  arc::mojom::IntentHelperInstance* const arc_intent_helper =
      GetArcIntentHelper(profile, kArcInstanceHelperVersionWithUrlListSupport);
  if (!arc_intent_helper)
    return false;

  mojo::Array<arc::mojom::UrlWithMimeTypePtr> urls;
  for (size_t i = 0; i < file_urls.size(); ++i) {
    GURL url;
    if (!ConvertToArcUrl(file_urls[i].path(), &url)) {
      LOG(ERROR) << "File on unsuppored path";
      return false;
    }

    arc::mojom::UrlWithMimeTypePtr url_with_type =
        arc::mojom::UrlWithMimeType::New();
    url_with_type->url = url.spec();
    url_with_type->mime_type = mime_types[i];
    urls.push_back(std::move(url_with_type));
  }
  arc_intent_helper->HandleUrlList(std::move(urls), task.app_id,
                                   StringToArcAction(task.action_id));
  return true;
}

}  // namespace file_tasks
}  // namespace file_manager
