// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_resource_providers.h"

#include "base/basictypes.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/balloon_host.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

////////////////////////////////////////////////////////////////////////////////
// TaskManagerNotificationResource class
////////////////////////////////////////////////////////////////////////////////

SkBitmap* TaskManagerNotificationResource::default_icon_ = NULL;

TaskManagerNotificationResource::TaskManagerNotificationResource(
    BalloonHost* balloon_host)
    : balloon_host_(balloon_host) {
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetBitmapNamed(IDR_PLUGIN);
  }
  process_handle_ =
      balloon_host_->tab_contents()->render_view_host()->process()->GetHandle();
  pid_ = base::GetProcId(process_handle_);
  title_ = l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_NOTIFICATION_PREFIX,
                                      balloon_host_->GetSource());
}

TaskManagerNotificationResource::~TaskManagerNotificationResource() {
}

string16 TaskManagerNotificationResource::GetTitle() const {
  return title_;
}

string16 TaskManagerNotificationResource::GetProfileName() const {
  return string16();
}

SkBitmap TaskManagerNotificationResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle TaskManagerNotificationResource::GetProcess() const {
  return process_handle_;
}

TaskManager::Resource::Type TaskManagerNotificationResource::GetType() const {
  return NOTIFICATION;
}

bool TaskManagerNotificationResource::CanInspect() const {
  return true;
}

void TaskManagerNotificationResource::Inspect() const {
  DevToolsWindow::OpenDevToolsWindow(
      balloon_host_->tab_contents()->render_view_host());
}

bool TaskManagerNotificationResource::SupportNetworkUsage() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerNotificationResourceProvider class
////////////////////////////////////////////////////////////////////////////////

// static
TaskManagerNotificationResourceProvider*
TaskManagerNotificationResourceProvider::Create(TaskManager* task_manager) {
  return new TaskManagerNotificationResourceProvider(task_manager);
}

TaskManagerNotificationResourceProvider::
    TaskManagerNotificationResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false) {
}

TaskManagerNotificationResourceProvider::
    ~TaskManagerNotificationResourceProvider() {
}

TaskManager::Resource* TaskManagerNotificationResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  // TODO(johnnyg): provide resources by pid if necessary.
  return NULL;
}

void TaskManagerNotificationResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Add all the existing BalloonHosts.
  BalloonCollection* collection =
      g_browser_process->notification_ui_manager()->balloon_collection();
  const BalloonCollection::Balloons& balloons = collection->GetActiveBalloons();
  for (BalloonCollection::Balloons::const_iterator it = balloons.begin();
       it != balloons.end(); ++it) {
    AddToTaskManager((*it)->view()->GetHost());
  }

  // Register for notifications about extension process changes.
  registrar_.Add(this, chrome::NOTIFICATION_NOTIFY_BALLOON_CONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED,
                 content::NotificationService::AllSources());
}

void TaskManagerNotificationResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications about extension process changes.
  registrar_.Remove(this, chrome::NOTIFICATION_NOTIFY_BALLOON_CONNECTED,
                    content::NotificationService::AllSources());
  registrar_.Remove(this, chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED,
                    content::NotificationService::AllSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());
  resources_.clear();
}

void TaskManagerNotificationResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_NOTIFY_BALLOON_CONNECTED:
      AddToTaskManager(content::Source<BalloonHost>(source).ptr());
      break;
    case chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED:
      RemoveFromTaskManager(content::Source<BalloonHost>(source).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

void TaskManagerNotificationResourceProvider::AddToTaskManager(
    BalloonHost* balloon_host) {
  TaskManagerNotificationResource* resource =
      new TaskManagerNotificationResource(balloon_host);
  DCHECK(resources_.find(balloon_host) == resources_.end());
  resources_[balloon_host] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerNotificationResourceProvider::RemoveFromTaskManager(
    BalloonHost* balloon_host) {
  if (!updating_)
    return;
  std::map<BalloonHost*, TaskManagerNotificationResource*>::iterator iter =
      resources_.find(balloon_host);
  if (iter == resources_.end())
    return;

  // Remove the resource from the Task Manager.
  TaskManagerNotificationResource* resource = iter->second;
  task_manager_->RemoveResource(resource);

  // Remove it from the map.
  resources_.erase(iter);

  // Finally, delete the resource.
  delete resource;
}
