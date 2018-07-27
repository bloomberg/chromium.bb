// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_

#include <array>
#include <map>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"
#include "url/origin.h"

namespace blink {
namespace mojom {
class StoragePartitionService;
}

namespace scheduler {
class WebThreadScheduler;
}
}  // namespace blink

namespace content {
class LocalStorageCachedArea;

// Keeps a map of all the LocalStorageCachedArea objects in a renderer. This is
// needed because we can have n LocalStorageArea objects for the same origin but
// we want just one LocalStorageCachedArea to service them (no point in having
// multiple caches of the same data in the same process).
// TODO(dmurph): Rename to remove LocalStorage.
class CONTENT_EXPORT LocalStorageCachedAreas {
 public:
  LocalStorageCachedAreas(
      blink::mojom::StoragePartitionService* storage_partition_service,
      blink::scheduler::WebThreadScheduler* main_thread_scheduler);
  ~LocalStorageCachedAreas();

  // Returns, creating if necessary, a cached storage area for the given origin.
  scoped_refptr<LocalStorageCachedArea>
      GetCachedArea(const url::Origin& origin);

  scoped_refptr<LocalStorageCachedArea> GetSessionStorageArea(
      const std::string& namespace_id,
      const url::Origin& origin);

  void CloneNamespace(const std::string& source_namespace,
                      const std::string& destination_namespace);

  size_t TotalCacheSize() const;

  void set_cache_limit_for_testing(size_t limit) {
    CHECK(sequence_checker_.CalledOnValidSequence());
    total_cache_limit_ = limit;
  }

 private:
  void ClearAreasIfNeeded();

  scoped_refptr<LocalStorageCachedArea> GetCachedArea(
      const std::string& namespace_id,
      const url::Origin& origin,
      blink::scheduler::WebThreadScheduler* scheduler);

  // TODO(dmurph): Remove release check when crashing has stopped.
  // http://crbug.com/857464
  base::SequenceCheckerImpl sequence_checker_;

  blink::mojom::StoragePartitionService* const storage_partition_service_;

  struct DOMStorageNamespace {
   public:
    DOMStorageNamespace();
    ~DOMStorageNamespace();
    DOMStorageNamespace(DOMStorageNamespace&& other);
    DOMStorageNamespace& operator=(DOMStorageNamespace&&) = default;

    void CheckPrefixes() const;

    size_t TotalCacheSize() const;
    // Returns true if this namespace is totally unused and can be deleted.
    bool CleanUpUnusedAreas();

    // TODO(dmurph): Remove the prefix & postfix after memory corruption is
    // solved.
    int64_t prefix;
    blink::mojom::SessionStorageNamespacePtr session_storage_namespace;
    base::flat_map<url::Origin, scoped_refptr<LocalStorageCachedArea>>
        cached_areas;
    int64_t postfix;

    DISALLOW_COPY_AND_ASSIGN(DOMStorageNamespace);
  };

  base::flat_map<std::string, DOMStorageNamespace> cached_namespaces_;
  size_t total_cache_limit_;

  // Not owned.
  blink::scheduler::WebThreadScheduler* main_thread_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageCachedAreas);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_
