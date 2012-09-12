// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_worker_resource_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/worker_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::WorkerService;

// Objects of this class are created on the IO thread and then passed to the UI
// thread where they are passed to the task manager. All methods must be called
// only on the UI thread. Destructor may be called on any thread.
class TaskManagerSharedWorkerResource : public TaskManager::Resource {
 public:
  TaskManagerSharedWorkerResource(const GURL& url,
                                  const string16& name,
                                  int process_id,
                                  int routing_id,
                                  base::ProcessHandle process_handle);
  virtual ~TaskManagerSharedWorkerResource();

  bool Matches(int process_id, int routing_id) const;

  void UpdateProcessHandle(base::ProcessHandle handle);
  base::ProcessHandle handle() const { return handle_; }
  int process_id() const { return process_id_; }

 private:
  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;

  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE;

  int process_id_;
  int routing_id_;
  string16 title_;
  base::ProcessHandle handle_;

  static gfx::ImageSkia* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerSharedWorkerResource);
};

gfx::ImageSkia* TaskManagerSharedWorkerResource::default_icon_ = NULL;

TaskManagerSharedWorkerResource::TaskManagerSharedWorkerResource(
    const GURL& url,
    const string16& name,
    int process_id,
    int routing_id,
    base::ProcessHandle process_handle)
    : process_id_(process_id),
      routing_id_(routing_id),
      handle_(process_handle) {
  title_ = UTF8ToUTF16(url.spec());
  if (!name.empty())
    title_ += ASCIIToUTF16(" (") + name + ASCIIToUTF16(")");
}

TaskManagerSharedWorkerResource::~TaskManagerSharedWorkerResource() {
}

bool TaskManagerSharedWorkerResource::Matches(int process_id,
                                              int routing_id) const {
  return process_id_ == process_id && routing_id_ == routing_id;
}

void TaskManagerSharedWorkerResource::UpdateProcessHandle(
    base::ProcessHandle handle) {
  handle_ = handle;
}

string16 TaskManagerSharedWorkerResource::GetTitle() const {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_WORKER_PREFIX, title_);
}

string16 TaskManagerSharedWorkerResource::GetProfileName() const {
  return string16();
}

gfx::ImageSkia TaskManagerSharedWorkerResource::GetIcon() const {
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
    // TODO(jabdelmalek): use different icon for web workers.
  }
  return *default_icon_;
}

base::ProcessHandle TaskManagerSharedWorkerResource::GetProcess() const {
  return handle_;
}

int TaskManagerSharedWorkerResource::GetUniqueChildProcessId() const {
  return process_id_;
}

TaskManager::Resource::Type TaskManagerSharedWorkerResource::GetType() const {
  return WORKER;
}

bool TaskManagerSharedWorkerResource::CanInspect() const {
  return true;
}

void TaskManagerSharedWorkerResource::Inspect() const {
  // TODO(yurys): would be better to get profile from one of the tabs connected
  // to the worker.
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return;
  DevToolsAgentHost* agent_host =
      DevToolsAgentHostRegistry::GetDevToolsAgentHostForWorker(
          process_id_, routing_id_);
  DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host);
}

bool TaskManagerSharedWorkerResource::SupportNetworkUsage() const {
  return false;
}

void TaskManagerSharedWorkerResource::SetSupportNetworkUsage() {
}


// This class is needed to ensure that all resources in WorkerResourceList are
// deleted if corresponding task is posted to but not executed on the UI
// thread.
class TaskManagerWorkerResourceProvider::WorkerResourceListHolder {
 public:
  WorkerResourceListHolder() {
  }

  ~WorkerResourceListHolder() {
    STLDeleteElements(&resources_);
  }

  WorkerResourceList* resources() {
    return &resources_;
  }

 private:
  WorkerResourceList resources_;
};


TaskManagerWorkerResourceProvider::
    TaskManagerWorkerResourceProvider(TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

TaskManagerWorkerResourceProvider::~TaskManagerWorkerResourceProvider() {
  DeleteAllResources();
}

TaskManager::Resource* TaskManagerWorkerResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  return NULL;
}

void TaskManagerWorkerResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;
  // Register for notifications to get new child processes.
  registrar_.Add(this, content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  // Get existing workers.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(
          &TaskManagerWorkerResourceProvider::StartObservingWorkers,
          this));
}

void TaskManagerWorkerResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;
  launching_workers_.clear();
  DeleteAllResources();
  registrar_.Remove(
      this, content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(
          &TaskManagerWorkerResourceProvider::StopObservingWorkers,
          this));
}

void TaskManagerWorkerResourceProvider::WorkerCreated(
    const GURL& url,
    const string16& name,
    int process_id,
    int route_id) {
  TaskManagerSharedWorkerResource* resource =
      new TaskManagerSharedWorkerResource(
          url, name, process_id, route_id, base::kNullProcessHandle);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskManagerWorkerResourceProvider::NotifyWorkerCreated,
                 this, base::Owned(new WorkerResourceHolder(resource))));
}

void TaskManagerWorkerResourceProvider::WorkerDestroyed(int process_id,
                                                        int route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          &TaskManagerWorkerResourceProvider::NotifyWorkerDestroyed,
          this, process_id, route_id));
}

void TaskManagerWorkerResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  content::ChildProcessData* process_data =
      content::Details<content::ChildProcessData>(details).ptr();
  if (process_data->type != content::PROCESS_TYPE_WORKER)
    return;
  if (type == content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED) {
    ProcessIdToWorkerResources::iterator it =
        launching_workers_.find(process_data->id);
    if (it == launching_workers_.end())
      return;
    WorkerResourceList& resources = it->second;
    for (WorkerResourceList::iterator r = resources.begin();
         r !=resources.end(); ++r) {
      (*r)->UpdateProcessHandle(process_data->handle);
      task_manager_->AddResource(*r);
    }
    launching_workers_.erase(it);
  } else if (type == content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED) {
    // Worker process may be destroyed before WorkerMsg_TerminateWorkerContex
    // message is handled and WorkerDestroyed is fired. In this case we won't
    // get WorkerDestroyed notification and have to clear resources for such
    // workers here when the worker process has been destroyed.
    for (WorkerResourceList::iterator it = resources_.begin();
         it !=resources_.end();) {
      if ((*it)->process_id() == process_data->id) {
        task_manager_->RemoveResource(*it);
        delete *it;
        it = resources_.erase(it);
      } else {
        ++it;
      }
    }
    DCHECK(launching_workers_.find(process_data->id) ==
           launching_workers_.end());
  }
}

void TaskManagerWorkerResourceProvider::NotifyWorkerCreated(
    WorkerResourceHolder* resource_holder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!updating_)
    return;
  AddResource(resource_holder->release());
}

void TaskManagerWorkerResourceProvider::NotifyWorkerDestroyed(
    int process_id, int routing_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!updating_)
    return;
  for (WorkerResourceList::iterator it = resources_.begin();
       it !=resources_.end(); ++it) {
    if ((*it)->Matches(process_id, routing_id)) {
      task_manager_->RemoveResource(*it);
      delete *it;
      resources_.erase(it);
      return;
    }
  }
}

void TaskManagerWorkerResourceProvider::StartObservingWorkers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_ptr<WorkerResourceListHolder> holder(new WorkerResourceListHolder);
  std::vector<WorkerService::WorkerInfo> worker_info =
      WorkerService::GetInstance()->GetWorkers();

  for (size_t i = 0; i < worker_info.size(); ++i) {
    holder->resources()->push_back(new TaskManagerSharedWorkerResource(
        worker_info[i].url, worker_info[i].name, worker_info[i].process_id,
        worker_info[i].route_id, worker_info[i].handle));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &TaskManagerWorkerResourceProvider::AddWorkerResourceList,
          this, base::Owned(holder.release())));

  WorkerService::GetInstance()->AddObserver(this);
}

void TaskManagerWorkerResourceProvider::StopObservingWorkers() {
  WorkerService::GetInstance()->RemoveObserver(this);
}

void TaskManagerWorkerResourceProvider::AddWorkerResourceList(
    WorkerResourceListHolder* resource_list_holder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!updating_)
    return;
  WorkerResourceList* resources = resource_list_holder->resources();
  for (WorkerResourceList::iterator it = resources->begin();
       it !=resources->end(); ++it) {
    AddResource(*it);
  }
  resources->clear();
}

void TaskManagerWorkerResourceProvider::AddResource(
    TaskManagerSharedWorkerResource* resource) {
  DCHECK(updating_);
  resources_.push_back(resource);
  if (resource->handle() == base::kNullProcessHandle) {
    int process_id = resource->process_id();
    launching_workers_[process_id].push_back(resource);
  } else {
    task_manager_->AddResource(resource);
  }
}

void TaskManagerWorkerResourceProvider::DeleteAllResources() {
  STLDeleteElements(&resources_);
}
