// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INSTANCE_HOLDER_H_
#define COMPONENTS_ARC_INSTANCE_HOLDER_H_

#include <string>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/arc/connection_notifier.h"
#include "components/arc/connection_observer.h"

// A macro to call InstanceHolder<T>::GetInstanceForVersionDoNotCallDirectly().
// In order to avoid exposing method names from within the Mojo bindings, we
// will rely on stringification and the fact that the method min versions have a
// consistent naming scheme.
#define ARC_GET_INSTANCE_FOR_METHOD(holder, method_name)        \
  (holder)->GetInstanceForVersionDoNotCallDirectly(             \
      std::remove_pointer<decltype(                             \
          holder)>::type::Instance::k##method_name##MinVersion, \
      #method_name)

namespace arc {

// Holds a Mojo instance+version pair. This also allows for listening for state
// changes for the particular instance. T should be a Mojo interface type
// (arc::mojom::XxxInstance).
template <typename T>
class InstanceHolder {
 public:
  using Observer = ConnectionObserver<T>;
  using Instance = T;

  InstanceHolder() = default;

  // Returns true if the Mojo interface is ready at least for its version 0
  // interface. Use an Observer if you want to be notified when this is ready.
  // This can only be called on the thread that this class was created on.
  bool has_instance() const { return instance_; }

  // Gets the Mojo interface that's intended to call for
  // |method_name_for_logging|, but only if its reported version is at least
  // |min_version|. Returns nullptr if the instance is either not ready or does
  // not have the requested version, and logs appropriately.
  // This function should not be called directly. Instead, use the
  // ARC_GET_INSTANCE_FOR_METHOD() macro.
  T* GetInstanceForVersionDoNotCallDirectly(
      uint32_t min_version,
      const char method_name_for_logging[]) {
    if (!instance_) {
      VLOG(1) << "Instance " << T::Name_ << " not available.";
      return nullptr;
    }
    if (version_ < min_version) {
      LOG(ERROR) << "Instance for " << T::Name_
                 << "::" << method_name_for_logging
                 << " version mismatch. Expected " << min_version << " got "
                 << version_;
      return nullptr;
    }
    return instance_;
  }

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on. RemoveObserver does nothing if |observer| is not in
  // the list.
  void AddObserver(Observer* observer) {
    connection_notifier_.AddObserver(observer);
    if (has_instance())
      connection_notifier_.NotifyConnectionReady();
  }

  void RemoveObserver(Observer* observer) {
    connection_notifier_.RemoveObserver(observer);
  }

  // Sets |instance| with |version|.
  // This can be called in both case; on ready, and on closed.
  // Passing nullptr to |instance| means closing.
  void SetInstance(T* instance, uint32_t version = T::Version_) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(instance == nullptr || instance_ == nullptr);

    // Note: This can be called with nullptr even if |instance_| is still
    // nullptr for just in case clean up purpose. No-op in such a case.
    if (instance == instance_)
      return;

    instance_ = instance;
    version_ = version;
    if (instance_)
      connection_notifier_.NotifyConnectionReady();
    else
      connection_notifier_.NotifyConnectionClosed();
  }

 private:
  // This class does not have ownership. The pointers should be managed by the
  // callee.
  T* instance_ = nullptr;
  uint32_t version_ = 0;

  THREAD_CHECKER(thread_checker_);
  internal::ConnectionNotifier connection_notifier_;

  DISALLOW_COPY_AND_ASSIGN(InstanceHolder<T>);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INSTANCE_HOLDER_H_
