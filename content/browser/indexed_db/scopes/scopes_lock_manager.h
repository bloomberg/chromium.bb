// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_
#define CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_

#include <iosfwd>
#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// Generic two-level lock management system based on ranges. Granted locks are
// represented by the |ScopeLock| class.
class CONTENT_EXPORT ScopesLockManager {
 public:
  // Shared locks can share access to a lock range, while exclusive locks
  // require that they are the only lock for their range.
  enum class LockType { kShared, kExclusive };

  // The range is [begin, end).
  struct CONTENT_EXPORT LockRange {
    LockRange(std::string begin, std::string end);
    LockRange() = default;
    ~LockRange() = default;
    std::string begin;
    std::string end;
  };

  // Represents a granted lock in the ScopesLockManager. When this object is
  // destroyed, the lock is released. Since default construction is supported,
  // |is_locked()| can be used to inquire locked status. Also, |Release()| can
  // be called to manually release the lock, which appropriately updates the
  // |is_locked()| result.
  class CONTENT_EXPORT ScopeLock {
   public:
    ScopeLock();
    ScopeLock(ScopeLock&&) noexcept;
    // The |closure| is called when the lock is released, either by destruction
    // of this object or by the |Released()| call. It will be called
    // synchronously on the sequence runner this lock is released on.
    ScopeLock(LockRange range, int level, base::OnceClosure closure);
    ~ScopeLock() = default;
    // This does NOT release the lock if one is being held.
    ScopeLock& operator=(ScopeLock&&) noexcept;

    // Returns true if this object is holding a lock.
    bool is_locked() const { return is_locked_; }

    // Releases this lock.
    void Release();

    int level() const { return level_; }
    const LockRange& range() const { return range_; }

   private:
    bool is_locked_ = false;
    LockRange range_;
    int level_ = 0;
    base::ScopedClosureRunner closure_runner_;

    DISALLOW_COPY_AND_ASSIGN(ScopeLock);
  };

  using LockAquiredCallback = base::OnceCallback<void(ScopeLock)>;

  ScopesLockManager() = default;

  virtual ~ScopesLockManager() = default;

  virtual int64_t LocksHeldForTesting() const = 0;
  virtual int64_t RequestsWaitingForTesting() const = 0;

  // Acquires a lock for a given lock level. Lock levels are treated as
  // completely independent domains. The lock levels start at zero.
  virtual void AcquireLock(int level,
                           const LockRange& range,
                           LockType type,
                           LockAquiredCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopesLockManager);
};

// Stream operator so lock range can be used in log statements.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const ScopesLockManager::LockRange& range);

CONTENT_EXPORT bool operator==(const ScopesLockManager::LockRange& x,
                               const ScopesLockManager::LockRange& y);
CONTENT_EXPORT bool operator!=(const ScopesLockManager::LockRange& x,
                               const ScopesLockManager::LockRange& y);

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_
