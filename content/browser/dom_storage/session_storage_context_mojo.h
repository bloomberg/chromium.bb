// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "url/origin.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace content {

// Used for mojo-based SessionStorage implementation.
class CONTENT_EXPORT SessionStorageContextMojo {
 public:
  SessionStorageContextMojo(service_manager::Connector* connector,
                            base::FilePath subdirectory);
  ~SessionStorageContextMojo();

  void OpenSessionStorage(int64_t namespace_id,
                          const url::Origin& origin,
                          mojom::LevelDBWrapperRequest request);

  void CreateSessionNamespace(int64_t namespace_id,
                              const std::string& persistent_namespace_id);
  void CloneSessionNamespace(int64_t namespace_id_to_clone,
                             int64_t clone_namespace_id,
                             const std::string& clone_persistent_namespace_id);
  void DeleteSessionNamespace(int64_t namespace_id, bool should_persist);

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
  const base::FilePath subdirectory_;

  base::WeakPtrFactory<SessionStorageContextMojo> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_
