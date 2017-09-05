// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_namespace.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/common/dom_storage/dom_storage_types.h"

namespace content {

DOMStorageNamespace::DOMStorageNamespace(
    const base::FilePath& directory,
    DOMStorageTaskRunner* task_runner)
    : namespace_id_(kLocalStorageNamespaceId),
      directory_(directory),
      task_runner_(task_runner) {
}

DOMStorageNamespace::DOMStorageNamespace(
    int64_t namespace_id,
    const std::string& persistent_namespace_id,
    SessionStorageDatabase* session_storage_database,
    DOMStorageTaskRunner* task_runner)
    : namespace_id_(namespace_id),
      persistent_namespace_id_(persistent_namespace_id),
      task_runner_(task_runner),
      session_storage_database_(session_storage_database) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
}

DOMStorageNamespace::~DOMStorageNamespace() {
}

DOMStorageArea* DOMStorageNamespace::OpenStorageArea(const GURL& origin) {
  if (AreaHolder* holder = GetAreaHolder(origin)) {
    ++(holder->open_count_);
#if defined(OS_ANDROID)
    if (holder->open_count_ > 1)
      holder->area_->SetCacheOnlyKeys(false);
#endif
    return holder->area_.get();
  }
  DOMStorageArea* area;
  if (namespace_id_ == kLocalStorageNamespaceId) {
    area = new DOMStorageArea(origin, directory_, task_runner_.get());
  } else {
    area = new DOMStorageArea(namespace_id_, persistent_namespace_id_, nullptr,
                              origin, session_storage_database_.get(),
                              task_runner_.get());
  }
  areas_[origin] = AreaHolder(area, 1);
  return area;
}

void DOMStorageNamespace::CloseStorageArea(DOMStorageArea* area) {
  AreaHolder* holder = GetAreaHolder(area->origin());
  DCHECK(holder);
  DCHECK_EQ(holder->area_.get(), area);
  --(holder->open_count_);
  // TODO(ssid): disable caching when the open count goes to 0 and it's safe,
  // crbug.com/743187.
}

DOMStorageArea* DOMStorageNamespace::GetOpenStorageArea(const GURL& origin) {
  AreaHolder* holder = GetAreaHolder(origin);
  if (holder && holder->open_count_)
    return holder->area_.get();
  return NULL;
}

DOMStorageNamespace* DOMStorageNamespace::Clone(
    int64_t clone_namespace_id,
    const std::string& clone_persistent_namespace_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, clone_namespace_id);
  DOMStorageNamespace* clone = new DOMStorageNamespace(
      clone_namespace_id, clone_persistent_namespace_id,
      session_storage_database_.get(), task_runner_.get());
  AreaMap::const_iterator it = areas_.begin();
  // Clone the in-memory structures.
  for (; it != areas_.end(); ++it) {
    DOMStorageArea* area = it->second.area_->ShallowCopy(
        clone_namespace_id, clone_persistent_namespace_id);
    clone->areas_[it->first] = AreaHolder(area, 0);
  }
  // And clone the on-disk structures, too.
  if (session_storage_database_) {
    auto clone_task = base::BindOnce(
        base::IgnoreResult(&SessionStorageDatabase::CloneNamespace),
        session_storage_database_, persistent_namespace_id_,
        clone_persistent_namespace_id);
    auto callback =
        base::BindOnce(&DOMStorageNamespace::OnCloneStorageDone, clone);
    task_runner_->GetSequencedTaskRunner(DOMStorageTaskRunner::COMMIT_SEQUENCE)
        ->PostTaskAndReply(FROM_HERE, std::move(clone_task),
                           std::move(callback));
  }
  return clone;
}

void DOMStorageNamespace::DeleteLocalStorageOrigin(const GURL& origin) {
  DCHECK(!session_storage_database_.get());
  AreaHolder* holder = GetAreaHolder(origin);
  if (holder) {
    holder->area_->DeleteOrigin();
    return;
  }
  if (!directory_.empty()) {
    scoped_refptr<DOMStorageArea> area =
        new DOMStorageArea(origin, directory_, task_runner_.get());
    area->DeleteOrigin();
  }
}

void DOMStorageNamespace::DeleteSessionStorageOrigin(const GURL& origin) {
  DOMStorageArea* area = OpenStorageArea(origin);
  area->FastClear();
  CloseStorageArea(area);
}

void DOMStorageNamespace::PurgeMemory(bool aggressively) {
  if (namespace_id_ == kLocalStorageNamespaceId && directory_.empty())
    return;  // We can't purge local storage w/o backing on disk.

  AreaMap::iterator it = areas_.begin();
  while (it != areas_.end()) {
    const AreaHolder& holder = it->second;

    // We can't purge if there are changes pending.
    if (holder.area_->HasUncommittedChanges()) {
      if (holder.open_count_ == 0) {
         // Schedule an immediate commit so the next time we're asked to purge,
         // we can drop it from memory.
         holder.area_->ScheduleImmediateCommit();
      }
      ++it;
      continue;
    }

    // If not in use, we can shut it down and remove
    // it from our collection entirely.
    if (holder.open_count_ == 0) {
      holder.area_->Shutdown();
      areas_.erase(it++);
      continue;
    }

    if (aggressively) {
      // If aggressive is true, we clear caches and such
      // for opened areas.
      holder.area_->PurgeMemory();
    }

    ++it;
  }
}

void DOMStorageNamespace::Shutdown() {
  AreaMap::const_iterator it = areas_.begin();
  for (; it != areas_.end(); ++it)
    it->second.area_->Shutdown();
}

void DOMStorageNamespace::Flush() {
  for (auto& entry : areas_) {
    if (!entry.second.area_->HasUncommittedChanges())
      continue;
    entry.second.area_->ScheduleImmediateCommit();
  }
}

DOMStorageNamespace::UsageStatistics DOMStorageNamespace::GetUsageStatistics()
    const {
  UsageStatistics stats = {0};
  for (AreaMap::const_iterator it = areas_.begin(); it != areas_.end(); ++it) {
    if (it->second.area_->map_memory_used()) {
      stats.total_cache_size += it->second.area_->map_memory_used();
      ++stats.total_area_count;
      if (it->second.open_count_ == 0)
        ++stats.inactive_area_count;
    }
  }
  return stats;
}

void DOMStorageNamespace::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  task_runner_->AssertIsRunningOnPrimarySequence();
  for (const auto& it : areas_)
    it.second.area_->OnMemoryDump(pmd);
}

void DOMStorageNamespace::GetOriginsWithAreas(
    std::vector<GURL>* origins) const {
  origins->clear();
  for (const auto& entry : areas_)
    origins->push_back(entry.first);
}

int DOMStorageNamespace::GetAreaOpenCount(const GURL& origin) const {
  const auto& found = areas_.find(origin);
  if (found == areas_.end())
    return 0;
  return found->second.open_count_;
}

DOMStorageNamespace::AreaHolder*
DOMStorageNamespace::GetAreaHolder(const GURL& origin) {
  AreaMap::iterator found = areas_.find(origin);
  if (found == areas_.end())
    return NULL;
  return &(found->second);
}

void DOMStorageNamespace::OnCloneStorageDone() {
  task_runner_->AssertIsRunningOnPrimarySequence();
  // Shallow copy of commit batches are no longer needed since the database is
  // cloned.
  for (AreaMap::const_iterator it = areas_.begin(); it != areas_.end(); ++it)
    it->second.area_->ClearShallowCopiedCommitBatches();
}

// AreaHolder

DOMStorageNamespace::AreaHolder::AreaHolder()
    : open_count_(0) {
}

DOMStorageNamespace::AreaHolder::AreaHolder(
    DOMStorageArea* area, int count)
    : area_(area), open_count_(count) {
}

DOMStorageNamespace::AreaHolder& DOMStorageNamespace::AreaHolder::operator=(
    AreaHolder&& other) {
  area_ = std::move(other.area_);
  open_count_ = other.open_count_;
  return *this;
}

DOMStorageNamespace::AreaHolder::~AreaHolder() {
}

}  // namespace content
