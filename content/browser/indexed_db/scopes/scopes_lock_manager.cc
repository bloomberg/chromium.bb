// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/scopes_lock_manager.h"

#include <ostream>

namespace content {

ScopesLockManager::LockRange::LockRange(std::string begin, std::string end)
    : begin(std::move(begin)), end(std::move(end)) {}

ScopesLockManager::ScopeLock::ScopeLock() = default;
ScopesLockManager::ScopeLock::ScopeLock(ScopeLock&& other) noexcept {
  DCHECK(!this->is_locked_) << "Cannot move a lock onto an active lock.";
  this->is_locked_ = other.is_locked_;
  this->range_ = std::move(other.range_);
  this->level_ = other.level_;
  this->closure_runner_ = std::move(other.closure_runner_);
  other.is_locked_ = false;
}
ScopesLockManager::ScopeLock::ScopeLock(LockRange range,
                                        int level,
                                        base::OnceClosure closure)
    : is_locked_(!closure.is_null()),
      range_(std::move(range)),
      level_(level),
      closure_runner_(std::move(closure)) {}

ScopesLockManager::ScopeLock& ScopesLockManager::ScopeLock::operator=(
    ScopesLockManager::ScopeLock&& other) noexcept {
  DCHECK(!this->is_locked_);
  this->is_locked_ = other.is_locked_;
  this->range_ = std::move(other.range_);
  this->level_ = other.level_;
  this->closure_runner_ = std::move(other.closure_runner_);
  other.is_locked_ = false;
  return *this;
};

void ScopesLockManager::ScopeLock::Release() {
  is_locked_ = false;
  closure_runner_.RunAndReset();
}

std::ostream& operator<<(std::ostream& out,
                         const ScopesLockManager::LockRange& range) {
  return out << "<ScopesLockManager::ScopeLock>{begin: " << range.begin
             << ", end: " << range.end << "}";
}

bool operator==(const ScopesLockManager::LockRange& x,
                const ScopesLockManager::LockRange& y) {
  return x.begin == y.begin && x.end == y.end;
}
bool operator!=(const ScopesLockManager::LockRange& x,
                const ScopesLockManager::LockRange& y) {
  return !(x == y);
}
}  // namespace content
