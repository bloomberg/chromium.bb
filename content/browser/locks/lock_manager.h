// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOCKS_LOCK_CONTEXT_H_
#define CONTENT_BROWSER_LOCKS_LOCK_CONTEXT_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/locks/lock_manager.mojom.h"
#include "url/origin.h"

namespace content {

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. An instance must only be used on the sequence
// it was created on.
class LockManager : public base::RefCountedThreadSafe<LockManager>,
                    public blink::mojom::LockManager {
 public:
  LockManager();

  void CreateService(blink::mojom::LockManagerRequest request);

  // Request a lock. When the lock is acquired, |callback| will be invoked with
  // a LockHandle.
  void RequestLock(const url::Origin& origin,
                   const std::string& name,
                   LockMode mode,
                   WaitMode wait,
                   blink::mojom::LockRequestPtr request) override;

  // Called by a LockHandle's implementation when destructed.
  void ReleaseLock(const url::Origin& origin, int64_t id);

 protected:
  friend class base::RefCountedThreadSafe<LockManager>;
  ~LockManager() override;

 private:
  // Internal representation of a lock request or held lock.
  struct Lock;

  // State for a particular origin.
  struct OriginState {
    OriginState();
    ~OriginState();

    bool IsGrantable(const std::string& name, LockMode mode) const;
    void MergeLockState(const std::string& name, LockMode mode);

    std::map<int64_t, std::unique_ptr<Lock>> requested;
    std::map<int64_t, std::unique_ptr<Lock>> held;

    // These sets represent what is held or requested, so that "IsGrantable"
    // tests are simple.
    std::unordered_set<std::string> shared;
    std::unordered_set<std::string> exclusive;
  };

  bool IsGrantable(const url::Origin& origin,
                   const std::string& name,
                   LockMode mode);

  // Called when a lock is requested and optionally when a lock is released,
  // to process outstanding requests within the origin.
  void ProcessRequests(const url::Origin& origin);

  mojo::BindingSet<blink::mojom::LockManager> bindings_;

  int64_t next_lock_id = 1;
  std::map<url::Origin, OriginState> origins_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LockManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LockManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOCKS_LOCK_CONTEXT_H
