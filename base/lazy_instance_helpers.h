// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LAZY_INSTANCE_INTERNAL_H_
#define BASE_LAZY_INSTANCE_INTERNAL_H_

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/logging.h"

// Helper methods used by LazyInstance and a few other base APIs for thread-safe
// lazy construction.

namespace base {
namespace internal {

// Our AtomicWord doubles as a spinlock, where a value of
// kLazyInstanceStateCreating means the spinlock is being held for creation.
constexpr subtle::AtomicWord kLazyInstanceStateCreating = 1;

// Helper for GetOrCreateLazyPointer(). Checks if instance needs to be created.
// If so returns true otherwise if another thread has beat us, waits for
// instance to be created and returns false.
BASE_EXPORT bool NeedsLazyInstance(subtle::AtomicWord* state);

// Helper for GetOrCreateLazyPointer(). After creating an instance, this is
// called to register the dtor to be called at program exit and to update the
// atomic state to hold the |new_instance|
BASE_EXPORT void CompleteLazyInstance(subtle::AtomicWord* state,
                                      subtle::AtomicWord new_instance,
                                      void (*destructor)(void*),
                                      void* destructor_arg);

// If |state| is uninitialized (zero), constructs a value using |creator_func|,
// stores it into |state| and registers |destructor| to be called with
// |destructor_arg| as argument when the current AtExitManager goes out of
// scope. Then, returns the value stored in |state|. It is safe to have
// concurrent calls to this function with the same |state|. |creator_func| may
// return nullptr if it doesn't want to create an instance anymore (e.g. on
// shutdown), it is from then on required to return nullptr to all callers (ref.
// StaticMemorySingletonTraits). Callers need to synchronize before
// |creator_func| may return a non-null instance again (ref.
// StaticMemorySingletonTraits::ResurectForTesting()).
template <typename CreatorFunc>
void* GetOrCreateLazyPointer(subtle::AtomicWord* state,
                             const CreatorFunc& creator_func,
                             void (*destructor)(void*),
                             void* destructor_arg) {
  DCHECK(state);

  // If any bit in the created mask is true, the instance has already been
  // fully constructed.
  constexpr subtle::AtomicWord kLazyInstanceCreatedMask =
      ~internal::kLazyInstanceStateCreating;

  // We will hopefully have fast access when the instance is already created.
  // Since a thread sees |state| == 0 or kLazyInstanceStateCreating at most
  // once, the load is taken out of NeedsLazyInstance() as a fast-path. The load
  // has acquire memory ordering as a thread which sees |state| > creating needs
  // to acquire visibility over the associated data. Pairing Release_Store is in
  // CompleteLazyInstance().
  if (!(subtle::Acquire_Load(state) & kLazyInstanceCreatedMask) &&
      NeedsLazyInstance(state)) {
    // This thread won the race and is now responsible for creating the instance
    // and storing it back into |state|.
    subtle::AtomicWord instance =
        reinterpret_cast<subtle::AtomicWord>(creator_func());
    CompleteLazyInstance(state, instance, destructor, destructor_arg);
  }
  return reinterpret_cast<void*>(subtle::NoBarrier_Load(state));
}

}  // namespace internal
}  // namespace base

#endif  // BASE_LAZY_INSTANCE_INTERNAL_H_
