// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_
#define CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_

#include <iosfwd>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/scopes/scope_lock.h"
#include "content/browser/indexed_db/scopes/scope_lock_range.h"
#include "content/common/content_export.h"

namespace content {

// Generic two-level lock management system based on ranges. Granted locks are
// represented by the |ScopeLock| class.
class CONTENT_EXPORT ScopesLockManager {
 public:
  using LocksAquiredCallback = base::OnceCallback<void(std::vector<ScopeLock>)>;

  // Shared locks can share access to a lock range, while exclusive locks
  // require that they are the only lock for their range.
  enum class LockType { kShared, kExclusive };

  ScopesLockManager();
  virtual ~ScopesLockManager();

  virtual int64_t LocksHeldForTesting() const = 0;
  virtual int64_t RequestsWaitingForTesting() const = 0;

  // Acquires locks for the given requests. Lock levels are treated as
  // completely independent domains. The lock levels start at zero.
  // Returns false if any of the lock ranges were invalid or an invarient was
  // broken.
  struct CONTENT_EXPORT ScopeLockRequest {
    ScopeLockRequest(int level, ScopeLockRange range, LockType type);
    ~ScopeLockRequest() = default;
    int level;
    ScopeLockRange range;
    LockType type;
  };
  virtual bool AcquireLocks(base::flat_set<ScopeLockRequest> lock_requests,
                            LocksAquiredCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopesLockManager);

  base::WeakPtrFactory<ScopesLockManager> weak_factory_;
};

CONTENT_EXPORT bool operator<(const ScopesLockManager::ScopeLockRequest& x,
                              const ScopesLockManager::ScopeLockRequest& y);
CONTENT_EXPORT bool operator==(const ScopesLockManager::ScopeLockRequest& x,
                               const ScopesLockManager::ScopeLockRequest& y);
CONTENT_EXPORT bool operator!=(const ScopesLockManager::ScopeLockRequest& x,
                               const ScopesLockManager::ScopeLockRequest& y);

// This is a proxy between a LevelDB Comparator and the std::less interface.
// It sorts using the |begin| entry of the ranges.
struct CONTENT_EXPORT ScopesLockRangeLessThan {
  ScopesLockRangeLessThan() = default;
  ~ScopesLockRangeLessThan() = default;
  explicit ScopesLockRangeLessThan(const leveldb::Comparator* comparator)
      : comparator_(comparator) {}
  bool operator()(const ScopeLockRange& a, const ScopeLockRange& b) const {
    DCHECK(comparator_);
    return comparator_->Compare(leveldb::Slice(a.begin),
                                leveldb::Slice(b.begin)) < 0;
  }
  const leveldb::Comparator* comparator_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_
