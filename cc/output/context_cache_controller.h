// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_CONTEXT_CACHE_CONTROLLER_H_
#define CC_OUTPUT_CONTEXT_CACHE_CONTROLLER_H_

#include <cstdint>
#include <memory>

#include "base/macros.h"
#include "cc/base/cc_export.h"

class GrContext;

namespace gpu {
class ContextSupport;
}

namespace cc {

// ContextCacheController manages clearing cached data on ContextProvider when
// appropriate. Currently, cache clearing happens when the ContextProvider
// transitions from visible to not visible. As a ContextProvider may have
// multiple clients, ContextCacheController tracks visibility across all
// clients and only cleans up when appropriate.
//
// Note: Virtuals on this function are for testing only. This function is not
// designed to have multiple implementations.
class CC_EXPORT ContextCacheController {
 public:
  class CC_EXPORT ScopedVisibility {
   public:
    ~ScopedVisibility();

   private:
    friend class ContextCacheController;
    ScopedVisibility();
    void Release();

    bool released_ = false;
  };

  explicit ContextCacheController(gpu::ContextSupport* context_support);
  virtual ~ContextCacheController();

  void SetGrContext(GrContext* gr_context);

  // Clients of the owning ContextProvider should call this function when they
  // become visible. The returned ScopedVisibility pointer must be passed back
  // to ClientBecameNotVisible or it will DCHECK in its destructor.
  virtual std::unique_ptr<ScopedVisibility> ClientBecameVisible();

  // When a client becomes not visible (either due to a visibility change or
  // because it is being deleted), it must pass back any ScopedVisibility
  // pointers it owns via this function.
  virtual void ClientBecameNotVisible(
      std::unique_ptr<ScopedVisibility> scoped_visibility);

 protected:
  std::unique_ptr<ScopedVisibility> CreateScopedVisibilityForTesting() const;
  void ReleaseScopedVisibilityForTesting(
      std::unique_ptr<ScopedVisibility> scoped_visibility) const;

 private:
  gpu::ContextSupport* context_support_;
  GrContext* gr_context_ = nullptr;

  uint32_t num_clients_visible_ = 0;
};

}  // namespace cc

#endif  // CC_OUTPUT_CONTEXT_CACHE_CONTROLLER_H_
