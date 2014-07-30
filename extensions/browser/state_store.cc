// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/state_store.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace {

// Delay, in seconds, before we should open the State Store database. We
// defer it to avoid slowing down startup. See http://crbug.com/161848
const int kInitDelaySeconds = 1;

std::string GetFullKey(const std::string& extension_id,
                       const std::string& key) {
  return extension_id + "." + key;
}

}  // namespace

namespace extensions {

// Helper class to delay tasks until we're ready to start executing them.
class StateStore::DelayedTaskQueue {
 public:
  DelayedTaskQueue() : ready_(false) {}
  ~DelayedTaskQueue() {}

  // Queues up a task for invoking once we're ready. Invokes immediately if
  // we're already ready.
  void InvokeWhenReady(base::Closure task);

  // Marks us ready, and invokes all pending tasks.
  void SetReady();

  // Return whether or not the DelayedTaskQueue is |ready_|.
  bool ready() const { return ready_; }

 private:
  bool ready_;
  std::vector<base::Closure> pending_tasks_;
};

void StateStore::DelayedTaskQueue::InvokeWhenReady(base::Closure task) {
  if (ready_) {
    task.Run();
  } else {
    pending_tasks_.push_back(task);
  }
}

void StateStore::DelayedTaskQueue::SetReady() {
  ready_ = true;

  for (size_t i = 0; i < pending_tasks_.size(); ++i)
    pending_tasks_[i].Run();
  pending_tasks_.clear();
}

StateStore::StateStore(content::BrowserContext* context,
                       const base::FilePath& db_path,
                       bool deferred_load)
    : db_path_(db_path),
      task_queue_(new DelayedTaskQueue()),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(context));

  if (deferred_load) {
    // Don't Init() until the first page is loaded or the embedder explicitly
    // requests it.
    registrar_.Add(
        this,
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllBrowserContextsAndSources());
  } else {
    Init();
  }
}

StateStore::StateStore(content::BrowserContext* context,
                       scoped_ptr<ValueStore> value_store)
    : store_(value_store.Pass()),
      task_queue_(new DelayedTaskQueue()),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(context));

  // This constructor is for testing. No need to delay Init.
  Init();
}

StateStore::~StateStore() {
}

void StateStore::RequestInitAfterDelay() {
  InitAfterDelay();
}

void StateStore::RegisterKey(const std::string& key) {
  registered_keys_.insert(key);
}

void StateStore::GetExtensionValue(const std::string& extension_id,
                                   const std::string& key,
                                   ReadCallback callback) {
  task_queue_->InvokeWhenReady(base::Bind(&ValueStoreFrontend::Get,
                                          base::Unretained(&store_),
                                          GetFullKey(extension_id, key),
                                          callback));
}

void StateStore::SetExtensionValue(const std::string& extension_id,
                                   const std::string& key,
                                   scoped_ptr<base::Value> value) {
  task_queue_->InvokeWhenReady(base::Bind(&ValueStoreFrontend::Set,
                                          base::Unretained(&store_),
                                          GetFullKey(extension_id, key),
                                          base::Passed(&value)));
}

void StateStore::RemoveExtensionValue(const std::string& extension_id,
                                      const std::string& key) {
  task_queue_->InvokeWhenReady(base::Bind(&ValueStoreFrontend::Remove,
                                          base::Unretained(&store_),
                                          GetFullKey(extension_id, key)));
}

bool StateStore::IsInitialized() const {
  return task_queue_->ready();
}

void StateStore::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME);
  registrar_.RemoveAll();
  InitAfterDelay();
}

void StateStore::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    bool from_ephemeral,
    const std::string& old_name) {
  RemoveKeysForExtension(extension->id());
}

void StateStore::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  RemoveKeysForExtension(extension->id());
}

void StateStore::Init() {
  // Could be called twice if InitAfterDelay() is requested explicitly by the
  // embedder in addition to internally after first page load.
  if (IsInitialized())
    return;

  if (!db_path_.empty())
    store_.Init(db_path_);
  task_queue_->SetReady();
}

void StateStore::InitAfterDelay() {
  if (IsInitialized())
    return;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StateStore::Init, AsWeakPtr()),
      base::TimeDelta::FromSeconds(kInitDelaySeconds));
}

void StateStore::RemoveKeysForExtension(const std::string& extension_id) {
  for (std::set<std::string>::iterator key = registered_keys_.begin();
       key != registered_keys_.end();
       ++key) {
    task_queue_->InvokeWhenReady(base::Bind(&ValueStoreFrontend::Remove,
                                            base::Unretained(&store_),
                                            GetFullKey(extension_id, *key)));
  }
}

}  // namespace extensions
