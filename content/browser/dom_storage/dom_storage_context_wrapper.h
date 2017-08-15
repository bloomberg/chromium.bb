// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/memory_coordinator_client.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/common/content_export.h"
#include "content/common/storage_partition_service.mojom.h"
#include "content/public/browser/dom_storage_context.h"
#include "url/origin.h"

namespace base {
class FilePath;
}

namespace service_manager {
class Connector;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace content {

class DOMStorageContextImpl;
class LocalStorageContextMojo;

// This is owned by Storage Partition and encapsulates all its dom storage
// state.
class CONTENT_EXPORT DOMStorageContextWrapper
    : public DOMStorageContext,
      public base::RefCountedThreadSafe<DOMStorageContextWrapper>,
      public base::MemoryCoordinatorClient {
 public:
  // If |data_path| is empty, nothing will be saved to disk.
  DOMStorageContextWrapper(
      service_manager::Connector* connector,
      const base::FilePath& data_path,
      const base::FilePath& local_partition_path,
      storage::SpecialStoragePolicy* special_storage_policy);

  // DOMStorageContext implementation.
  void GetLocalStorageUsage(
      const GetLocalStorageUsageCallback& callback) override;
  void GetSessionStorageUsage(
      const GetSessionStorageUsageCallback& callback) override;
  void DeleteLocalStorageForPhysicalOrigin(const GURL& origin) override;
  void DeleteLocalStorage(const GURL& origin) override;
  void DeleteSessionStorage(const SessionStorageUsageInfo& usage_info) override;
  void SetSaveSessionStorageOnDisk() override;
  scoped_refptr<SessionStorageNamespace> RecreateSessionStorage(
      const std::string& persistent_id) override;
  void StartScavengingUnusedSessionStorage() override;

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState();

  // Called when the BrowserContext/Profile is going away.
  void Shutdown();

  void Flush();

  // See mojom::StoragePartitionService interface.
  void OpenLocalStorage(const url::Origin& origin,
                        mojom::LevelDBWrapperRequest request);

  void SetLocalStorageDatabaseForTesting(
      leveldb::mojom::LevelDBDatabaseAssociatedPtr database);

 private:
  friend class DOMStorageMessageFilter;  // for access to context()
  friend class SessionStorageNamespaceImpl;  // ditto
  friend class base::RefCountedThreadSafe<DOMStorageContextWrapper>;
  friend class MojoDOMStorageBrowserTest;

  ~DOMStorageContextWrapper() override;
  DOMStorageContextImpl* context() const { return context_.get(); }

  // Called on UI thread when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // base::MemoryCoordinatorClient implementation:
  void OnPurgeMemory() override;

  void PurgeMemory(DOMStorageContextImpl::PurgeOption purge_option);

  // Keep all mojo-ish details together and not bleed them through the public
  // interface. The |mojo_state_| object is owned by this object, but destroyed
  // asynchronously on the |mojo_task_runner_|.
  LocalStorageContextMojo* mojo_state_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner_;

  // To receive memory pressure signals.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  scoped_refptr<DOMStorageContextImpl> context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
