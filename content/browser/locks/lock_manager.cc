// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/locks/lock_manager.h"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

using blink::mojom::LockMode;

namespace content {

namespace {

// A LockHandle is passed to the client when a lock is granted. As long as the
// handle is held, the lock is held. Dropping the handle - either explicitly
// by script or by process termination - causes the lock to be released.
class LockHandleImpl final : public blink::mojom::LockHandle {
 public:
  static blink::mojom::LockHandlePtr Create(base::WeakPtr<LockManager> context,
                                            const url::Origin& origin,
                                            int64_t lock_id) {
    blink::mojom::LockHandlePtr ptr;
    mojo::MakeStrongBinding(
        std::make_unique<LockHandleImpl>(std::move(context), origin, lock_id),
        mojo::MakeRequest(&ptr));
    return ptr;
  }

  LockHandleImpl(base::WeakPtr<LockManager> context,
                 const url::Origin& origin,
                 int64_t lock_id)
      : context_(context), origin_(origin), lock_id_(lock_id) {}

  ~LockHandleImpl() override {
    if (context_)
      context_->ReleaseLock(origin_, lock_id_);
  }

 private:
  base::WeakPtr<LockManager> context_;
  const url::Origin origin_;
  const int64_t lock_id_;

  DISALLOW_COPY_AND_ASSIGN(LockHandleImpl);
};

}  // namespace

// A requested or held lock. When granted, a LockHandle will be minted
// and passed to the held callback. Eventually the client will drop the
// handle, which will notify the context and remove this.
struct LockManager::Lock {
  Lock(const std::string& name,
       LockMode mode,
       int64_t lock_id,
       const std::string& client_id,
       blink::mojom::LockRequestPtr request)
      : name(name),
        mode(mode),
        lock_id(lock_id),
        client_id(client_id),
        request(std::move(request)) {}

  ~Lock() = default;

  const std::string name;
  const LockMode mode;
  const int64_t lock_id;
  const std::string client_id;
  blink::mojom::LockRequestPtr request;
};

LockManager::LockManager() : weak_ptr_factory_(this) {}

LockManager::~LockManager() = default;

class LockManager::OriginState {
 public:
  OriginState() = default;
  ~OriginState() = default;

  void AddRequest(int64_t lock_id,
                  const std::string& name,
                  LockMode mode,
                  const std::string& client_id,
                  blink::mojom::LockRequestPtr request) {
    requested_.emplace(std::make_pair(
        lock_id, std::make_unique<Lock>(name, mode, lock_id, client_id,
                                        std::move(request))));
  }

  bool EraseLock(int64_t lock_id) {
    return requested_.erase(lock_id) || held_.erase(lock_id);
  }

  bool IsEmpty() const { return requested_.empty() && held_.empty(); }

  bool IsGrantable(const std::string& name, LockMode mode) const {
    if (mode == LockMode::EXCLUSIVE) {
      return !shared_.count(name) && !exclusive_.count(name);
    } else {
      return !exclusive_.count(name);
    }
  }

  void ProcessRequests(LockManager* lock_manager, const url::Origin& origin) {
    if (requested_.empty())
      return;

    shared_.clear();
    exclusive_.clear();
    for (const auto& id_lock_pair : held_) {
      const auto& lock = id_lock_pair.second;
      MergeLockState(lock->name, lock->mode);
    }

    for (auto it = requested_.begin(); it != requested_.end();) {
      auto& lock = it->second;

      bool granted = IsGrantable(lock->name, lock->mode);

      MergeLockState(lock->name, lock->mode);

      if (granted) {
        std::unique_ptr<Lock> grantee = std::move(lock);
        it = requested_.erase(it);
        grantee->request->Granted(
            LockHandleImpl::Create(lock_manager->weak_ptr_factory_.GetWeakPtr(),
                                   origin, grantee->lock_id));
        grantee->request = nullptr;
        held_.insert(std::make_pair(grantee->lock_id, std::move(grantee)));
      } else {
        ++it;
      }
    }
  }

  std::vector<blink::mojom::LockInfoPtr> SnapshotRequested() const {
    std::vector<blink::mojom::LockInfoPtr> out;
    out.reserve(requested_.size());
    for (const auto& id_lock_pair : requested_) {
      const auto& lock = id_lock_pair.second;
      out.emplace_back(base::in_place, lock->name, lock->mode, lock->client_id);
    }
    return out;
  }

  std::vector<blink::mojom::LockInfoPtr> SnapshotHeld() const {
    std::vector<blink::mojom::LockInfoPtr> out;
    out.reserve(held_.size());
    for (const auto& id_lock_pair : held_) {
      const auto& lock = id_lock_pair.second;
      out.emplace_back(base::in_place, lock->name, lock->mode, lock->client_id);
    }
    return out;
  }

 private:
  void MergeLockState(const std::string& name, LockMode mode) {
    (mode == LockMode::SHARED ? shared_ : exclusive_).insert(name);
  }

  // These represent the actual state of locks in the origin.
  std::map<int64_t, std::unique_ptr<Lock>> requested_;
  std::map<int64_t, std::unique_ptr<Lock>> held_;

  // These sets represent what is held or requested, so that "IsGrantable"
  // tests are simple. These are cleared/rebuilt when the request queue
  // is processed.
  std::unordered_set<std::string> shared_;
  std::unordered_set<std::string> exclusive_;
};

void LockManager::CreateService(blink::mojom::LockManagerRequest request,
                                const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(jsbell): This should reflect the 'environment id' from HTML,
  // and be the same opaque string seen in Service Worker client ids.
  const std::string client_id = base::GenerateGUID();

  bindings_.AddBinding(this, std::move(request), {origin, client_id});
}

void LockManager::RequestLock(const std::string& name,
                              LockMode mode,
                              WaitMode wait,
                              blink::mojom::LockRequestPtr request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = bindings_.dispatch_context();
  if (wait == WaitMode::NO_WAIT && !IsGrantable(context.origin, name, mode)) {
    request->Failed();
    return;
  }

  int64_t lock_id = next_lock_id++;
  request.set_connection_error_handler(base::BindOnce(&LockManager::ReleaseLock,
                                                      base::Unretained(this),
                                                      context.origin, lock_id));
  origins_[context.origin].AddRequest(lock_id, name, mode, context.client_id,
                                      std::move(request));
  ProcessRequests(context.origin);
}

void LockManager::ReleaseLock(const url::Origin& origin, int64_t lock_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::ContainsKey(origins_, origin))
    return;
  OriginState& state = origins_[origin];

  bool dirty = state.EraseLock(lock_id);

  if (state.IsEmpty())
    origins_.erase(origin);
  else if (dirty)
    ProcessRequests(origin);
}

void LockManager::QueryState(QueryStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const url::Origin& origin = bindings_.dispatch_context().origin;
  if (!base::ContainsKey(origins_, origin))
    return;
  OriginState& state = origins_[origin];

  std::move(callback).Run(state.SnapshotRequested(), state.SnapshotHeld());
}

bool LockManager::IsGrantable(const url::Origin& origin,
                              const std::string& name,
                              LockMode mode) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return true;
  return it->second.IsGrantable(name, mode);
}

void LockManager::ProcessRequests(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::ContainsKey(origins_, origin))
    return;

  OriginState& state = origins_[origin];
  DCHECK(!state.IsEmpty());
  state.ProcessRequests(this, origin);
  DCHECK(!state.IsEmpty());
}

}  // namespace content
