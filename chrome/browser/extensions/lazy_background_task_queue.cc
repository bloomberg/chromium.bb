// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/lazy_background_task_queue.h"

#include "base/callback.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

LazyBackgroundTaskQueue::LazyBackgroundTaskQueue(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

LazyBackgroundTaskQueue::~LazyBackgroundTaskQueue() {
}

bool LazyBackgroundTaskQueue::ShouldEnqueueTask(
    Profile* profile, const Extension* extension) {
  DCHECK(extension);
  if (extension->has_lazy_background_page()) {
    ExtensionProcessManager* pm = profile->GetExtensionProcessManager();
    ExtensionHost* background_host =
        pm->GetBackgroundHostForExtension(extension->id());
    if (!background_host || !background_host->did_stop_loading())
      return true;
  }

  return false;
}

void LazyBackgroundTaskQueue::AddPendingTask(
    Profile* profile,
    const std::string& extension_id,
    const PendingTask& task) {
  PendingTasksList* tasks_list = NULL;
  PendingTasksKey key(profile, extension_id);
  PendingTasksMap::iterator it = pending_tasks_.find(key);
  if (it == pending_tasks_.end()) {
    tasks_list = new PendingTasksList();
    pending_tasks_[key] = linked_ptr<PendingTasksList>(tasks_list);

    // If this is the first enqueued task, ensure the background page
    // is loaded.
    const Extension* extension = profile->GetExtensionService()->
        extensions()->GetByID(extension_id);
    DCHECK(extension->has_lazy_background_page());
  ExtensionProcessManager* pm = profile->GetExtensionProcessManager();
    pm->IncrementLazyKeepaliveCount(extension);
    pm->CreateBackgroundHost(extension, extension->GetBackgroundURL());
  } else {
    tasks_list = it->second.get();
  }

  tasks_list->push_back(task);
}

void LazyBackgroundTaskQueue::ProcessPendingTasks(ExtensionHost* host) {
  PendingTasksKey key(host->profile(), host->extension()->id());
  PendingTasksMap::iterator map_it = pending_tasks_.find(key);
  if (map_it == pending_tasks_.end()) {
    NOTREACHED();  // lazy page should not load without any pending tasks
    return;
  }

  PendingTasksList* tasks = map_it->second.get();
  for (PendingTasksList::const_iterator it = tasks->begin();
       it != tasks->end(); ++it) {
    it->Run(host);
  }

  tasks->clear();
  pending_tasks_.erase(map_it);

  // Balance the keepalive in AddPendingTask.
  host->profile()->GetExtensionProcessManager()->
      DecrementLazyKeepaliveCount(host->extension());
}

void LazyBackgroundTaskQueue::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
      // If an on-demand background page finished loading, dispatch queued up
      // events for it.
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      if (host->profile()->IsSameProfile(profile_) &&
          host->extension_host_type() ==
              chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE &&
          host->extension()->has_lazy_background_page()) {
        CHECK(host->did_stop_loading());
        ProcessPendingTasks(host);
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      // Clear pending tasks for this extension.
      Profile* profile = content::Source<Profile>(source).ptr();
      UnloadedExtensionInfo* unloaded =
          content::Details<UnloadedExtensionInfo>(details).ptr();
      pending_tasks_.erase(PendingTasksKey(
          profile, unloaded->extension->id()));
      if (profile->HasOffTheRecordProfile())
        pending_tasks_.erase(PendingTasksKey(
            profile->GetOffTheRecordProfile(), unloaded->extension->id()));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace extensions
