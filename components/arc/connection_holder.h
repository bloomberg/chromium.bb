// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_CONNECTION_HOLDER_H_
#define COMPONENTS_ARC_CONNECTION_HOLDER_H_

#include <string>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/arc/connection_notifier.h"
#include "components/arc/connection_observer.h"

// A macro to call
// ConnectionHolder<T>::GetInstanceForVersionDoNotCallDirectly(). In order to
// avoid exposing method names from within the Mojo bindings, we will rely on
// stringification and the fact that the method min versions have a consistent
// naming scheme.
#define ARC_GET_INSTANCE_FOR_METHOD(holder, method_name)        \
  (holder)->GetInstanceForVersionDoNotCallDirectly(             \
      std::remove_pointer<decltype(                             \
          holder)>::type::Instance::k##method_name##MinVersion, \
      #method_name)

namespace arc {

namespace internal {

// This is the core part of ConnectionHolder, which implementes the connection
// initialization.
// Currently, this supports only single direction connection. When Instance
// connection is established from ARC container via ArcBridgeService,
// the Instance proxy will be set.
// TODO(hidehiko): Support full-duplex connection.
template <typename InstanceType>
class ConnectionHolderImpl {
 public:
  explicit ConnectionHolderImpl(ConnectionNotifier* connection_notifier)
      : connection_notifier_(connection_notifier) {}

  InstanceType* instance() { return instance_; }
  uint32_t instance_version() const { return instance_version_; }

  // For single direction connection, when Instance proxy is obtained,
  // it means connected.
  bool IsConnected() const { return instance_; }

  // Sets (or resets if |instance| is nullptr) the instance.
  void SetInstance(InstanceType* instance,
                   uint32_t version = InstanceType::version_) {
    DCHECK(instance == nullptr || instance_ == nullptr);

    // Note: This can be called with nullptr even if |instance_| is still
    // nullptr for just in case clean up purpose. No-op in such a case.
    if (instance == instance_)
      return;

    instance_ = instance;
    instance_version_ = version;
    if (instance_)
      connection_notifier_->NotifyConnectionReady();
    else
      connection_notifier_->NotifyConnectionClosed();
  }

 private:
  // This class does not have ownership. The pointers should be managed by the
  // caller.
  ConnectionNotifier* const connection_notifier_;
  InstanceType* instance_ = nullptr;
  uint32_t instance_version_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ConnectionHolderImpl);
};

}  // namespace internal

// Holds a Mojo instance+version pair. This also allows for listening for state
// changes for the particular instance. InstanceType should be a Mojo
// interface type (arc::mojom::XxxInstance).
template <typename InstanceType>
class ConnectionHolder {
 public:
  using Observer = ConnectionObserver<InstanceType>;
  using Instance = InstanceType;

  ConnectionHolder() = default;

  // Returns true if the Mojo interface is ready at least for its version 0
  // interface. Use an Observer if you want to be notified when this is ready.
  // This can only be called on the thread that this class was created on.
  bool IsConnected() const { return impl_.IsConnected(); }

  // Gets the Mojo interface that's intended to call for
  // |method_name_for_logging|, but only if its reported version is at least
  // |min_version|. Returns nullptr if the instance is either not ready or does
  // not have the requested version, and logs appropriately.
  // This function should not be called directly. Instead, use the
  // ARC_GET_INSTANCE_FOR_METHOD() macro.
  InstanceType* GetInstanceForVersionDoNotCallDirectly(
      uint32_t min_version,
      const char method_name_for_logging[]) {
    if (!impl_.IsConnected()) {
      VLOG(1) << "Instance " << InstanceType::Name_ << " not available.";
      return nullptr;
    }
    if (impl_.instance_version() < min_version) {
      LOG(ERROR) << "Instance for " << InstanceType::Name_
                 << "::" << method_name_for_logging
                 << " version mismatch. Expected " << min_version << " got "
                 << impl_.instance_version();
      return nullptr;
    }
    return impl_.instance();
  }

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on. RemoveObserver does nothing if |observer| is not in
  // the list.
  void AddObserver(Observer* observer) {
    connection_notifier_.AddObserver(observer);
    if (impl_.IsConnected())
      connection_notifier_.NotifyConnectionReady();
  }

  void RemoveObserver(Observer* observer) {
    connection_notifier_.RemoveObserver(observer);
  }

  // Sets |instance| with |version|.
  // This can be called in both case; on ready, and on closed.
  // Passing nullptr to |instance| means closing.
  void SetInstance(InstanceType* instance,
                   uint32_t version = InstanceType::Version_) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    impl_.SetInstance(instance, version);
  }

 private:
  THREAD_CHECKER(thread_checker_);
  internal::ConnectionNotifier connection_notifier_;
  internal::ConnectionHolderImpl<InstanceType> impl_{&connection_notifier_};

  DISALLOW_COPY_AND_ASSIGN(ConnectionHolder);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_CONNECTION_HOLDER_H_
