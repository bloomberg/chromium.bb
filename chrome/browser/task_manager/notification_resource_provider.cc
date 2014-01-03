// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/notification_resource_provider.h"

#include "base/strings/string16.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/notifications/balloon_host.h"
#include "chrome/browser/notifications/balloon_notification_ui_manager.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace task_manager {

class NotificationResource : public Resource {
 public:
  explicit NotificationResource(BalloonHost* balloon_host);
  virtual ~NotificationResource();

  // Resource interface
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual base::string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE { }

 private:
  // The icon painted for notifications.       .
  static gfx::ImageSkia* default_icon_;

  // Non-owned pointer to the balloon host.
  BalloonHost* balloon_host_;

  // Cached data about the balloon host.
  base::ProcessHandle process_handle_;
  int pid_;
  int unique_process_id_;
  base::string16 title_;

  DISALLOW_COPY_AND_ASSIGN(NotificationResource);
};

gfx::ImageSkia* NotificationResource::default_icon_ = NULL;

NotificationResource::NotificationResource(BalloonHost* balloon_host)
    : balloon_host_(balloon_host) {
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
  }
  process_handle_ =
      balloon_host_->web_contents()->GetRenderProcessHost()->GetHandle();
  unique_process_id_ =
      balloon_host_->web_contents()->GetRenderProcessHost()->GetID();
  pid_ = base::GetProcId(process_handle_);
  title_ = l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_NOTIFICATION_PREFIX,
                                      balloon_host_->GetSource());
}

NotificationResource::~NotificationResource() {
}

base::string16 NotificationResource::GetTitle() const {
  return title_;
}

base::string16 NotificationResource::GetProfileName() const {
  return base::string16();
}

gfx::ImageSkia NotificationResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle NotificationResource::GetProcess() const {
  return process_handle_;
}

int NotificationResource::GetUniqueChildProcessId() const {
  return unique_process_id_;
}

Resource::Type NotificationResource::GetType() const {
  return NOTIFICATION;
}

bool NotificationResource::CanInspect() const {
  return true;
}

void NotificationResource::Inspect() const {
  DevToolsWindow::OpenDevToolsWindow(
      balloon_host_->web_contents()->GetRenderViewHost());
}

bool NotificationResource::SupportNetworkUsage() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NotificationResourceProvider class
////////////////////////////////////////////////////////////////////////////////

// static
NotificationResourceProvider*
NotificationResourceProvider::Create(TaskManager* task_manager) {
  return new NotificationResourceProvider(task_manager);
}

NotificationResourceProvider::
    NotificationResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false) {
}

NotificationResourceProvider::~NotificationResourceProvider() {
}

Resource* NotificationResourceProvider::GetResource(
    int origin_pid,
    int child_id,
    int route_id) {
  // TODO(johnnyg): provide resources by pid if necessary.
  return NULL;
}

void NotificationResourceProvider::StartUpdating() {
  // MessageCenter does not use Balloons.
  if (NotificationUIManager::DelegatesToMessageCenter())
    return;

  DCHECK(!updating_);
  updating_ = true;

  // Add all the existing BalloonHosts.
  BalloonNotificationUIManager* balloon_manager =
      static_cast<BalloonNotificationUIManager*>(
          g_browser_process->notification_ui_manager());
  BalloonCollection* collection = balloon_manager->balloon_collection();
  const BalloonCollection::Balloons& balloons =
      collection->GetActiveBalloons();
  for (BalloonCollection::Balloons::const_iterator it = balloons.begin();
       it != balloons.end(); ++it) {
    BalloonHost* balloon_host = (*it)->balloon_view()->GetHost();
    if (balloon_host)
      AddToTaskManager(balloon_host);
  }
  // Register for notifications about extension process changes.
  registrar_.Add(this, chrome::NOTIFICATION_NOTIFY_BALLOON_CONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED,
                 content::NotificationService::AllSources());
}

void NotificationResourceProvider::StopUpdating() {
  // MessageCenter does not use Balloons.
  if (NotificationUIManager::DelegatesToMessageCenter())
    return;

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

void NotificationResourceProvider::Observe(
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

void NotificationResourceProvider::AddToTaskManager(
    BalloonHost* balloon_host) {
  NotificationResource* resource = new NotificationResource(balloon_host);
  DCHECK(resources_.find(balloon_host) == resources_.end());
  resources_[balloon_host] = resource;
  task_manager_->AddResource(resource);
}

void NotificationResourceProvider::RemoveFromTaskManager(
    BalloonHost* balloon_host) {
  if (!updating_)
    return;
  std::map<BalloonHost*, NotificationResource*>::iterator iter =
      resources_.find(balloon_host);
  if (iter == resources_.end())
    return;

  // Remove the resource from the Task Manager.
  NotificationResource* resource = iter->second;
  task_manager_->RemoveResource(resource);

  // Remove it from the map.
  resources_.erase(iter);

  // Finally, delete the resource.
  delete resource;
}

}  // namespace task_manager
