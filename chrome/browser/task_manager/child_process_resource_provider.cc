// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/child_process_resource_provider.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/process_resource_usage.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/service_registry.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::WebContents;

namespace task_manager {

class ChildProcessResource : public Resource {
 public:
  ChildProcessResource(int process_type,
                       const base::string16& name,
                       base::ProcessHandle handle,
                       int unique_process_id);
  ~ChildProcessResource() override;

  // Resource methods:
  base::string16 GetTitle() const override;
  base::string16 GetProfileName() const override;
  gfx::ImageSkia GetIcon() const override;
  base::ProcessHandle GetProcess() const override;
  int GetUniqueChildProcessId() const override;
  Type GetType() const override;
  bool SupportNetworkUsage() const override;
  void SetSupportNetworkUsage() override;
  void Refresh() override;
  bool ReportsV8MemoryStats() const override;
  size_t GetV8MemoryAllocated() const override;
  size_t GetV8MemoryUsed() const override;

  // Returns the pid of the child process.
  int process_id() const { return pid_; }

 private:
  // Returns a localized title for the child process.  For example, a plugin
  // process would be "Plugin: Flash" when name is "Flash".
  base::string16 GetLocalizedTitle() const;

  static void ConnectResourceReporterOnIOThread(
      int id,
      mojo::InterfaceRequest<mojom::ResourceUsageReporter> req);

  int process_type_;
  base::string16 name_;
  base::ProcessHandle handle_;
  int pid_;
  int unique_process_id_;
  mutable base::string16 title_;
  bool network_usage_support_;
  std::unique_ptr<ProcessResourceUsage> resource_usage_;

  // The icon painted for the child processs.
  // TODO(jcampan): we should have plugin specific icons for well-known
  // plugins.
  static gfx::ImageSkia* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessResource);
};

gfx::ImageSkia* ChildProcessResource::default_icon_ = NULL;

// static
void ChildProcessResource::ConnectResourceReporterOnIOThread(
    int id,
    mojo::InterfaceRequest<mojom::ResourceUsageReporter> req) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::BrowserChildProcessHost* host =
      content::BrowserChildProcessHost::FromID(id);
  if (!host)
    return;

  content::ServiceRegistry* registry = host->GetServiceRegistry();
  if (!registry)
    return;

  registry->ConnectToRemoteService(std::move(req));
}

ChildProcessResource::ChildProcessResource(int process_type,
                                           const base::string16& name,
                                           base::ProcessHandle handle,
                                           int unique_process_id)
    : process_type_(process_type),
      name_(name),
      handle_(handle),
      unique_process_id_(unique_process_id),
      network_usage_support_(false) {
  // We cache the process id because it's not cheap to calculate, and it won't
  // be available when we get the plugin disconnected notification.
  pid_ = base::GetProcId(handle);
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
    // TODO(jabdelmalek): use different icon for web workers.
  }
  mojom::ResourceUsageReporterPtr service;
  mojo::InterfaceRequest<mojom::ResourceUsageReporter> request =
      mojo::GetProxy(&service);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChildProcessResource::ConnectResourceReporterOnIOThread,
                 unique_process_id, base::Passed(&request)));
  resource_usage_.reset(new ProcessResourceUsage(std::move(service)));
}

ChildProcessResource::~ChildProcessResource() {
}

// Resource methods:
base::string16 ChildProcessResource::GetTitle() const {
  if (title_.empty())
    title_ = GetLocalizedTitle();

  return title_;
}

base::string16 ChildProcessResource::GetProfileName() const {
  return base::string16();
}

gfx::ImageSkia ChildProcessResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle ChildProcessResource::GetProcess() const {
  return handle_;
}

int ChildProcessResource::GetUniqueChildProcessId() const {
  return unique_process_id_;
}

Resource::Type ChildProcessResource::GetType() const {
  // Translate types to Resource::Type, since ChildProcessData's type
  // is not available for all TaskManager resources.
  switch (process_type_) {
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
    case content::PROCESS_TYPE_PPAPI_BROKER:
      return Resource::PLUGIN;
    case content::PROCESS_TYPE_UTILITY:
      return Resource::UTILITY;
    case content::PROCESS_TYPE_ZYGOTE:
      return Resource::ZYGOTE;
    case content::PROCESS_TYPE_SANDBOX_HELPER:
      return Resource::SANDBOX_HELPER;
    case content::PROCESS_TYPE_GPU:
      return Resource::GPU;
    case PROCESS_TYPE_NACL_LOADER:
    case PROCESS_TYPE_NACL_BROKER:
      return Resource::NACL;
    default:
      return Resource::UNKNOWN;
  }
}

bool ChildProcessResource::SupportNetworkUsage() const {
  return network_usage_support_;
}

void ChildProcessResource::SetSupportNetworkUsage() {
  network_usage_support_ = true;
}

base::string16 ChildProcessResource::GetLocalizedTitle() const {
  base::string16 title = name_;
  if (title.empty()) {
    switch (process_type_) {
      case content::PROCESS_TYPE_PPAPI_PLUGIN:
      case content::PROCESS_TYPE_PPAPI_BROKER:
        title = l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UNKNOWN_PLUGIN_NAME);
        break;
      default:
        // Nothing to do for non-plugin processes.
        break;
    }
  }

  // Explicitly mark name as LTR if there is no strong RTL character,
  // to avoid the wrong concatenation result similar to "!Yahoo Mail: the
  // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
  // or Arabic word for "plugin".
  base::i18n::AdjustStringForLocaleDirection(&title);

  switch (process_type_) {
    case content::PROCESS_TYPE_UTILITY:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX, title);
    case content::PROCESS_TYPE_GPU:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_GPU_PREFIX);
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PLUGIN_PREFIX, title);
    case content::PROCESS_TYPE_PPAPI_BROKER:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PLUGIN_BROKER_PREFIX,
                                        title);
    case PROCESS_TYPE_NACL_BROKER:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NACL_BROKER_PREFIX);
    case PROCESS_TYPE_NACL_LOADER:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_NACL_PREFIX, title);
    // These types don't need display names or get them from elsewhere.
    case content::PROCESS_TYPE_BROWSER:
    case content::PROCESS_TYPE_RENDERER:
    case content::PROCESS_TYPE_ZYGOTE:
    case content::PROCESS_TYPE_SANDBOX_HELPER:
    case content::PROCESS_TYPE_MAX:
      NOTREACHED();
      break;
    case content::PROCESS_TYPE_UNKNOWN:
      NOTREACHED() << "Need localized name for child process type.";
  }

  return title;
}

void ChildProcessResource::Refresh() {
  if (resource_usage_)
    resource_usage_->Refresh(base::Closure());
}

bool ChildProcessResource::ReportsV8MemoryStats() const {
  if (resource_usage_)
    return resource_usage_->ReportsV8MemoryStats();
  return false;
}

size_t ChildProcessResource::GetV8MemoryAllocated() const {
  if (resource_usage_)
    return resource_usage_->GetV8MemoryAllocated();
  return 0;
}

size_t ChildProcessResource::GetV8MemoryUsed() const {
  if (resource_usage_)
    return resource_usage_->GetV8MemoryUsed();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// ChildProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

ChildProcessResourceProvider::
    ChildProcessResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false) {
}

ChildProcessResourceProvider::~ChildProcessResourceProvider() {
}

Resource* ChildProcessResourceProvider::GetResource(
    int origin_pid,
    int child_id,
    int route_id) {
  PidResourceMap::iterator iter = pid_to_resources_.find(origin_pid);
  if (iter != pid_to_resources_.end())
    return iter->second;
  else
    return NULL;
}

void ChildProcessResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Get the existing child processes.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ChildProcessResourceProvider::RetrieveChildProcessData,
          this));

  BrowserChildProcessObserver::Add(this);
}

void ChildProcessResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
  pid_to_resources_.clear();

  BrowserChildProcessObserver::Remove(this);
}

void ChildProcessResourceProvider::BrowserChildProcessHostConnected(
    const content::ChildProcessData& data) {
  DCHECK(updating_);

  if (resources_.count(data.handle)) {
    // The case may happen that we have added a child_process_info as part of
    // the iteration performed during StartUpdating() call but the notification
    // that it has connected was not fired yet. So when the notification
    // happens, we already know about this plugin and just ignore it.
    return;
  }
  AddToTaskManager(data);
}

void ChildProcessResourceProvider::
    BrowserChildProcessHostDisconnected(
        const content::ChildProcessData& data) {
  DCHECK(updating_);

  ChildProcessMap::iterator iter = resources_.find(data.handle);
  if (iter == resources_.end()) {
    // ChildProcessData disconnection notifications are asynchronous, so we
    // might be notified for a plugin we don't know anything about (if it was
    // closed before the task manager was shown and destroyed after that).
    return;
  }
  // Remove the resource from the Task Manager.
  ChildProcessResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  // Remove it from the provider.
  resources_.erase(iter);
  // Remove it from our pid map.
  PidResourceMap::iterator pid_iter =
      pid_to_resources_.find(resource->process_id());
  DCHECK(pid_iter != pid_to_resources_.end());
  if (pid_iter != pid_to_resources_.end())
    pid_to_resources_.erase(pid_iter);

  // Finally, delete the resource.
  delete resource;
}

void ChildProcessResourceProvider::AddToTaskManager(
    const content::ChildProcessData& child_process_data) {
  ChildProcessResource* resource =
      new ChildProcessResource(
          child_process_data.process_type,
          child_process_data.name,
          child_process_data.handle,
          child_process_data.id);
  resources_[child_process_data.handle] = resource;
  pid_to_resources_[resource->process_id()] = resource;
  task_manager_->AddResource(resource);
}

// The ChildProcessData::Iterator has to be used from the IO thread.
void ChildProcessResourceProvider::RetrieveChildProcessData() {
  std::vector<content::ChildProcessData> child_processes;
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // Only add processes which are already started, since we need their handle.
    if (iter.GetData().handle == base::kNullProcessHandle)
      continue;
    child_processes.push_back(iter.GetData());
  }
  // Now notify the UI thread that we have retrieved information about child
  // processes.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ChildProcessResourceProvider::ChildProcessDataRetreived,
          this, child_processes));
}

// This is called on the UI thread.
void ChildProcessResourceProvider::ChildProcessDataRetreived(
    const std::vector<content::ChildProcessData>& child_processes) {
  for (size_t i = 0; i < child_processes.size(); ++i)
    AddToTaskManager(child_processes[i]);

  task_manager_->model()->NotifyDataReady();
}

}  // namespace task_manager
