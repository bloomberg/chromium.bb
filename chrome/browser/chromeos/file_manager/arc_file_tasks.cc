// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/arc_file_tasks.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

namespace {

constexpr char kAppIdSeparator = '/';

// Converts an Android intent action (see kIntentAction* in
// components/arc/intent_helper/intent_constants.h) to a file task action ID
// (see chrome/browser/chromeos/file_manager/file_tasks.h).
std::string ArcActionToFileTaskActionId(const std::string& action) {
  if (action == arc::kIntentActionView)
    return "view";
  else if (action == arc::kIntentActionSend)
    return "send";
  else if (action == arc::kIntentActionSendMultiple)
    return "send_multiple";
  NOTREACHED() << "Unhandled ARC action \"" << action << "\"";
  return "";
}

// TODO(derat): Replace this with a FileTaskActionIdToArcAction method once
// HandleUrlList has been updated to take a string action rather than an
// ArcActionType.
arc::mojom::ActionType FileTaskActionIdToArcActionType(const std::string& id) {
  if (id == "view")
    return arc::mojom::ActionType::VIEW;
  if (id == "send")
    return arc::mojom::ActionType::SEND;
  if (id == "send_multiple")
    return arc::mojom::ActionType::SEND_MULTIPLE;
  NOTREACHED() << "Unhandled file task action ID \"" << id << "\"";
  return arc::mojom::ActionType::VIEW;
}

std::string ActivityNameToAppId(const std::string& package_name,
                                const std::string& activity_name) {
  return package_name + kAppIdSeparator + activity_name;
}

arc::mojom::ActivityNamePtr AppIdToActivityName(const std::string& id) {
  arc::mojom::ActivityNamePtr name = arc::mojom::ActivityName::New();

  const size_t separator = id.find(kAppIdSeparator);
  if (separator == std::string::npos) {
    name->package_name = id;
    name->activity_name = std::string();
  } else {
    name->package_name = id.substr(0, separator);
    name->activity_name = id.substr(separator + 1);
  }
  return name;
}

// Below is the sequence of thread-hopping for loading ARC file tasks.
void OnArcHandlerList(
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    const FindTasksCallback& callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);

void OnArcIconLoaded(
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    const FindTasksCallback& callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);

// Called after the handlers from ARC is obtained. Proceeds to OnArcIconLoaded.
void OnArcHandlerList(
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    const FindTasksCallback& callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* intent_helper_bridge =
      arc::ArcServiceManager::GetGlobalService<arc::ArcIntentHelperBridge>();
  if (!intent_helper_bridge) {
    callback.Run(std::move(result_list));
    return;
  }

  std::vector<arc::mojom::IntentHandlerInfoPtr> handlers_filtered =
      arc::ArcIntentHelperBridge::FilterOutIntentHelper(std::move(handlers));
  std::vector<arc::ArcIntentHelperBridge::ActivityName> activity_names;
  for (const arc::mojom::IntentHandlerInfoPtr& handler : handlers_filtered)
    activity_names.emplace_back(handler->package_name, handler->activity_name);

  intent_helper_bridge->GetActivityIcons(
      activity_names, base::Bind(&OnArcIconLoaded, base::Passed(&result_list),
                                 callback, base::Passed(&handlers_filtered)));
}

// Called after icon data for ARC apps are loaded. Proceeds to OnArcIconEncoded.
void OnArcIconLoaded(
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    const FindTasksCallback& callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using extensions::api::file_manager_private::Verb;
  for (const arc::mojom::IntentHandlerInfoPtr& handler : handlers) {
    std::string action(arc::kIntentActionView);
    if (handler->action.has_value())
      action = *handler->action;
    std::string name(handler->name);
    Verb handler_verb = Verb::VERB_NONE;
    if (action == arc::kIntentActionSend ||
        action == arc::kIntentActionSendMultiple) {
      handler_verb = Verb::VERB_SHARE_WITH;
    }
    auto it = icons->find(arc::ArcIntentHelperBridge::ActivityName(
        handler->package_name, handler->activity_name));
    const GURL& icon_url =
        (it == icons->end() ? GURL::EmptyGURL()
                            : it->second.icon16_dataurl->data);
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(
            ActivityNameToAppId(handler->package_name, handler->activity_name),
            TASK_TYPE_ARC_APP, ArcActionToFileTaskActionId(action)),
        name, handler_verb, icon_url, false /* is_default */,
        action != arc::kIntentActionView /* is_generic */));
  }
  callback.Run(std::move(result_list));
}

}  // namespace

void FindArcTasks(Profile* profile,
                  const std::vector<extensions::EntryInfo>& entries,
                  std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
                  const FindTasksCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  arc::mojom::IntentHelperInstance* arc_intent_helper = nullptr;
  // File manager in secondary profile cannot access ARC.
  if (chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      arc_intent_helper = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          RequestUrlListHandlerList);
    }
  }
  if (!arc_intent_helper) {
    callback.Run(std::move(result_list));
    return;
  }

  std::vector<arc::mojom::UrlWithMimeTypePtr> urls;
  for (const extensions::EntryInfo& entry : entries) {
    if (entry.is_directory) {  // ARC apps don't support directories.
      callback.Run(std::move(result_list));
      return;
    }

    GURL url;
    if (!util::ConvertPathToArcUrl(entry.path, &url)) {
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(file_urls.size(), mime_types.size());

  arc::mojom::IntentHelperInstance* arc_intent_helper = nullptr;
  // File manager in secondary profile cannot access ARC.
  if (chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      arc_intent_helper = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrlList);
    }
  }
  if (!arc_intent_helper)
    return false;

  std::vector<arc::mojom::UrlWithMimeTypePtr> urls;
  for (size_t i = 0; i < file_urls.size(); ++i) {
    GURL url;
    if (!util::ConvertPathToArcUrl(file_urls[i].path(), &url)) {
      LOG(ERROR) << "File on unsuppored path";
      return false;
    }

    arc::mojom::UrlWithMimeTypePtr url_with_type =
        arc::mojom::UrlWithMimeType::New();
    url_with_type->url = url.spec();
    url_with_type->mime_type = mime_types[i];
    urls.push_back(std::move(url_with_type));
  }

  arc_intent_helper->HandleUrlList(
      std::move(urls), AppIdToActivityName(task.app_id),
      FileTaskActionIdToArcActionType(task.action_id));
  return true;
}

}  // namespace file_tasks
}  // namespace file_manager
