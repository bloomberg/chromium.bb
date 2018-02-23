// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "content/common/storage_partition_service.mojom.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace content {

// Used for mojo-based SessionStorage implementation.
class CONTENT_EXPORT SessionStorageContextMojo {
 public:
  using GetStorageUsageCallback =
      base::OnceCallback<void(std::vector<SessionStorageUsageInfo>)>;

  SessionStorageContextMojo(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      service_manager::Connector* connector,
      base::Optional<base::FilePath> partition_directory,
      std::string leveldb_name);
  ~SessionStorageContextMojo();

  void OpenSessionStorage(int process_id,
                          const std::string& namespace_id,
                          mojom::SessionStorageNamespaceRequest request);

  void CreateSessionNamespace(const std::string& namespace_id);
  void CloneSessionNamespace(const std::string& namespace_id_to_clone,
                             const std::string& clone_namespace_id);
  void DeleteSessionNamespace(const std::string& namespace_id,
                              bool should_persist);
  void Flush();

  void GetStorageUsage(GetStorageUsageCallback callback);
  void DeleteStorage(const GURL& origin,
                     const std::string& persistent_namespace_id);

  // Called when the owning BrowserContext is ending.
  // Schedules the commit of any unsaved changes and will delete
  // and keep data on disk per the content settings and special storage
  // policies.
  void ShutdownAndDelete();

  // Clears any caches, to free up as much memory as possible. Next access to
  // storage for a particular origin will reload the data from the database.
  void PurgeMemory();

  // Clears unused leveldb wrappers, when thresholds are reached.
  void PurgeUnusedWrappersIfNeeded();

  base::WeakPtr<SessionStorageContextMojo> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
  const base::Optional<base::FilePath> partition_directory_path_;
  std::string leveldb_name_;

  base::WeakPtrFactory<SessionStorageContextMojo> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_
