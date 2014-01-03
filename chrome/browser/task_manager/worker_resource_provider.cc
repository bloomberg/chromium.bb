// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/worker_resource_provider.h"

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/worker_service.h"
#include "content/public/common/process_type.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using content::WorkerService;

namespace task_manager {

// Objects of this class are created on the IO thread and then passed to the UI
// thread where they are passed to the task manager. All methods must be called
// only on the UI thread. Destructor may be called on any thread.
class SharedWorkerResource : public Resource {
 public:
  SharedWorkerResource(const GURL& url,
                       const base::string16& name,
                       int process_id,
                       int routing_id,
                       base::ProcessHandle process_handle);
  virtual ~SharedWorkerResource();

  bool Matches(int process_id, int routing_id) const;

  void UpdateProcessHandle(base::ProcessHandle handle);
  base::ProcessHandle handle() const { return handle_; }
  int process_id() const { return process_id_; }

 private:
  // Resource methods:
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual base::string16 GetProfileName() const OVERRIDE;
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
  base::string16 title_;
  base::ProcessHandle handle_;

  static gfx::ImageSkia* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerResource);
};

gfx::ImageSkia* SharedWorkerResource::default_icon_ = NULL;

SharedWorkerResource::SharedWorkerResource(
    const GURL& url,
    const base::string16& name,
    int process_id,
    int routing_id,
    base::ProcessHandle process_handle)
    : process_id_(process_id),
      routing_id_(routing_id),
      handle_(process_handle) {
  title_ = base::UTF8ToUTF16(url.spec());
  if (!name.empty())
    title_ += base::ASCIIToUTF16(" (") + name + base::ASCIIToUTF16(")");
}

SharedWorkerResource::~SharedWorkerResource() {
}

bool SharedWorkerResource::Matches(int process_id,
                                   int routing_id) const {
  return process_id_ == process_id && routing_id_ == routing_id;
}

void SharedWorkerResource::UpdateProcessHandle(base::ProcessHandle handle) {
  handle_ = handle;
}

base::string16 SharedWorkerResource::GetTitle() const {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_WORKER_PREFIX, title_);
}

base::string16 SharedWorkerResource::GetProfileName() const {
  return base::string16();
}

gfx::ImageSkia SharedWorkerResource::GetIcon() const {
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
    // TODO(jabdelmalek): use different icon for web workers.
  }
  return *default_icon_;
}

base::ProcessHandle SharedWorkerResource::GetProcess() const {
  return handle_;
}

int SharedWorkerResource::GetUniqueChildProcessId() const {
  return process_id_;
}

Resource::Type SharedWorkerResource::GetType() const {
  return WORKER;
}

bool SharedWorkerResource::CanInspect() const {
  return true;
}

void SharedWorkerResource::Inspect() const {
  // TODO(yurys): would be better to get profile from one of the tabs connected
  // to the worker.
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return;
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForWorker(process_id_, routing_id_));
  DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host.get());
}

bool SharedWorkerResource::SupportNetworkUsage() const {
  return false;
}

void SharedWorkerResource::SetSupportNetworkUsage() {
}


// This class is needed to ensure that all resources in WorkerResourceList are
// deleted if corresponding task is posted to but not executed on the UI
// thread.
class WorkerResourceProvider::WorkerResourceListHolder {
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


WorkerResourceProvider::
    WorkerResourceProvider(TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

WorkerResourceProvider::~WorkerResourceProvider() {
  DeleteAllResources();
}

Resource* WorkerResourceProvider::GetResource(
    int origin_pid,
    int child_id,
    int route_id) {
  return NULL;
}

void WorkerResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;
  // Get existing workers.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(
          &WorkerResourceProvider::StartObservingWorkers,
          this));

  BrowserChildProcessObserver::Add(this);
}

void WorkerResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;
  launching_workers_.clear();
  DeleteAllResources();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(
          &WorkerResourceProvider::StopObservingWorkers,
          this));

  BrowserChildProcessObserver::Remove(this);
}

void WorkerResourceProvider::BrowserChildProcessHostConnected(
    const content::ChildProcessData& data) {
  DCHECK(updating_);

  if (data.process_type != content::PROCESS_TYPE_WORKER)
    return;

  ProcessIdToWorkerResources::iterator it(launching_workers_.find(data.id));
  if (it == launching_workers_.end())
    return;
  WorkerResourceList& resources = it->second;
  for (WorkerResourceList::iterator r = resources.begin();
       r != resources.end(); ++r) {
    (*r)->UpdateProcessHandle(data.handle);
    task_manager_->AddResource(*r);
  }
  launching_workers_.erase(it);
}

void WorkerResourceProvider::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  DCHECK(updating_);

  if (data.process_type != content::PROCESS_TYPE_WORKER)
    return;

  // Worker process may be destroyed before WorkerMsg_TerminateWorkerContex
  // message is handled and WorkerDestroyed is fired. In this case we won't
  // get WorkerDestroyed notification and have to clear resources for such
  // workers here when the worker process has been destroyed.
  for (WorkerResourceList::iterator it = resources_.begin();
       it != resources_.end();) {
    if ((*it)->process_id() == data.id) {
      task_manager_->RemoveResource(*it);
      delete *it;
      it = resources_.erase(it);
    } else {
      ++it;
    }
  }
  DCHECK(!ContainsKey(launching_workers_, data.id));
}

void WorkerResourceProvider::WorkerCreated(
    const GURL& url,
    const base::string16& name,
    int process_id,
    int route_id) {
  SharedWorkerResource* resource = new SharedWorkerResource(
      url, name, process_id, route_id, base::kNullProcessHandle);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WorkerResourceProvider::NotifyWorkerCreated,
                 this, base::Owned(new WorkerResourceHolder(resource))));
}

void WorkerResourceProvider::WorkerDestroyed(int process_id, int route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          &WorkerResourceProvider::NotifyWorkerDestroyed,
          this, process_id, route_id));
}

void WorkerResourceProvider::NotifyWorkerCreated(
    WorkerResourceHolder* resource_holder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!updating_)
    return;
  AddResource(resource_holder->release());
}

void WorkerResourceProvider::NotifyWorkerDestroyed(
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

void WorkerResourceProvider::StartObservingWorkers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_ptr<WorkerResourceListHolder> holder(new WorkerResourceListHolder);
  std::vector<WorkerService::WorkerInfo> worker_info =
      WorkerService::GetInstance()->GetWorkers();

  for (size_t i = 0; i < worker_info.size(); ++i) {
    holder->resources()->push_back(new SharedWorkerResource(
        worker_info[i].url, worker_info[i].name, worker_info[i].process_id,
        worker_info[i].route_id, worker_info[i].handle));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WorkerResourceProvider::AddWorkerResourceList,
          this, base::Owned(holder.release())));

  WorkerService::GetInstance()->AddObserver(this);
}

void WorkerResourceProvider::StopObservingWorkers() {
  WorkerService::GetInstance()->RemoveObserver(this);
}

void WorkerResourceProvider::AddWorkerResourceList(
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

void WorkerResourceProvider::AddResource(SharedWorkerResource* resource) {
  DCHECK(updating_);
  resources_.push_back(resource);
  if (resource->handle() == base::kNullProcessHandle) {
    int process_id = resource->process_id();
    launching_workers_[process_id].push_back(resource);
  } else {
    task_manager_->AddResource(resource);
  }
}

void WorkerResourceProvider::DeleteAllResources() {
  STLDeleteElements(&resources_);
}

}  // namespace task_manager
