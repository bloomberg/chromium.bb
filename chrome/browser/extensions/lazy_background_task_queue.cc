// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/lazy_background_task_queue.h"

#include "base/callback.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/view_type.h"
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
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

LazyBackgroundTaskQueue::~LazyBackgroundTaskQueue() {
}

bool LazyBackgroundTaskQueue::ShouldEnqueueTask(
    Profile* profile, const Extension* extension) {
  DCHECK(extension);
  if (BackgroundInfo::HasBackgroundPage(extension)) {
    ExtensionProcessManager* pm = extensions::ExtensionSystem::Get(profile)->
        process_manager();
    ExtensionHost* background_host =
        pm->GetBackgroundHostForExtension(extension->id());
    if (!background_host || !background_host->did_stop_loading())
      return true;
    if (pm->IsBackgroundHostClosing(extension->id()))
      pm->CancelSuspend(extension);
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

    const Extension* extension =
        ExtensionSystem::Get(profile)->extension_service()->
            extensions()->GetByID(extension_id);
    if (extension && BackgroundInfo::HasLazyBackgroundPage(extension)) {
      // If this is the first enqueued task, and we're not waiting for the
      // background page to unload, ensure the background page is loaded.
      ExtensionProcessManager* pm =
          ExtensionSystem::Get(profile)->process_manager();
      pm->IncrementLazyKeepaliveCount(extension);
      pm->CreateBackgroundHost(extension,
                               BackgroundInfo::GetBackgroundURL(extension));
    }
  } else {
    tasks_list = it->second.get();
  }

  tasks_list->push_back(task);
}

void LazyBackgroundTaskQueue::ProcessPendingTasks(
    ExtensionHost* host,
    Profile* profile,
    const Extension* extension) {
  if (!profile->IsSameProfile(profile_))
    return;

  PendingTasksKey key(profile, extension->id());
  PendingTasksMap::iterator map_it = pending_tasks_.find(key);
  if (map_it == pending_tasks_.end()) {
    if (BackgroundInfo::HasLazyBackgroundPage(extension))
      CHECK(!host);  // lazy page should not load without any pending tasks
    return;
  }

  // Swap the pending tasks to a temporary, to avoid problems if the task
  // list is modified during processing.
  PendingTasksList tasks;
  tasks.swap(*map_it->second);
  for (PendingTasksList::const_iterator it = tasks.begin();
       it != tasks.end(); ++it) {
    it->Run(host);
  }

  pending_tasks_.erase(key);

  // Balance the keepalive in AddPendingTask. Note we don't do this on a
  // failure to load, because the keepalive count is reset in that case.
  if (host && BackgroundInfo::HasLazyBackgroundPage(extension)) {
    ExtensionSystem::Get(profile)->process_manager()->
        DecrementLazyKeepaliveCount(extension);
  }
}

void LazyBackgroundTaskQueue::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
      // If an on-demand background page finished loading, dispatch queued up
      // events for it.
      ExtensionHost* host =
          content::Details<ExtensionHost>(details).ptr();
      if (host->extension_host_type() ==
              chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        CHECK(host->did_stop_loading());
        ProcessPendingTasks(host, host->profile(), host->extension());
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      // Notify consumers about the load failure when the background host dies.
      // This can happen if the extension crashes. This is not strictly
      // necessary, since we also unload the extension in that case (which
      // dispatches the tasks below), but is a good extra precaution.
      Profile* profile = content::Source<Profile>(source).ptr();
      ExtensionHost* host =
           content::Details<ExtensionHost>(details).ptr();
      if (host->extension_host_type() ==
              chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        ProcessPendingTasks(NULL, profile, host->extension());
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      // Notify consumers that the page failed to load.
      Profile* profile = content::Source<Profile>(source).ptr();
      UnloadedExtensionInfo* unloaded =
          content::Details<UnloadedExtensionInfo>(details).ptr();
      ProcessPendingTasks(NULL, profile, unloaded->extension);
      if (profile->HasOffTheRecordProfile()) {
        ProcessPendingTasks(NULL, profile->GetOffTheRecordProfile(),
                            unloaded->extension);
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace extensions
