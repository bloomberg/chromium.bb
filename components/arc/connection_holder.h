// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_CONNECTION_HOLDER_H_
#define COMPONENTS_ARC_CONNECTION_HOLDER_H_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/arc/connection_notifier.h"
#include "components/arc/connection_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"

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

// Small helper to implement HasInit<> trait below.
// Note that only Check() declaration is needed.
// - If InstanceType::Init exists, Check(InstanceType()) would return
//   std::true_type, because 1) the template param is successfully substituted,
//   and 2) Check(...) is weaker than the template's so there is no overload
//   conflict.
// - If not, Check(InstanceType()) would return std::false_type, because
//   template param substitution is failed, but it won't be compile error
//   thanks to SFINAE, thus Check(...) is the only candidate.
struct HasInitImpl {
  template <typename InstanceType>
  static auto Check(InstanceType* v)
      -> decltype(&InstanceType::Init, std::true_type());
  static std::false_type Check(...);
};

// Type trait to return whether InstanceType has Init() or not.
template <typename InstanceType>
using HasInit =
    decltype(HasInitImpl::Check(static_cast<InstanceType*>(nullptr)));

// Same as above, but for InstanceType::InitDeprecated.
struct HasInitDeprecatedImpl {
  template <typename InstanceType>
  static auto Check(InstanceType* v)
      -> decltype(&InstanceType::InitDeprecated, std::true_type());
  static std::false_type Check(...);
};

// Type trait to return whether InstanceType has InitDeprecated() or not.
template <typename InstanceType>
using HasInitDeprecated =
    decltype(HasInitDeprecatedImpl::Check(static_cast<InstanceType*>(nullptr)));

// Templates to count the number of arguments in a function. This is used to
// distinguish between interfaces that do not declare an Init() method, a
// one-argument (single-duplex) Init() and the two-argument (full-duplex)
// Init().
// TODO(crbug.com/750563): Simplify the templates once InitDeprecated() is
// removed.
template <typename Signature>
struct CountInitArgsImpl;

template <typename R, typename Receiver, typename... Args>
struct CountInitArgsImpl<R (Receiver::*)(Args...)> {
  static constexpr size_t value = sizeof...(Args);
};

template <class T>
struct Void {
  typedef void type;
};

template <typename T, typename U = void>
struct CountInitArgs {
  static constexpr size_t value = 0;
};

template <typename T>
struct CountInitArgs<T, typename Void<decltype(&T::Init)>::type> {
  static constexpr size_t value = CountInitArgsImpl<decltype(&T::Init)>::value;
};

// Full duplex Mojo connection holder implementation.
// InstanceType and HostType are Mojo interface types (arc::mojom::XxxInstance,
// and arc::mojom::XxxHost respectively).
template <typename InstanceType, typename HostType>
class ConnectionHolderImpl {
 public:
  explicit ConnectionHolderImpl(ConnectionNotifier* connection_notifier)
      : connection_notifier_(connection_notifier), weak_ptr_factory_(this) {}

  InstanceType* instance() { return instance_; }
  uint32_t instance_version() const { return instance_version_; }

  // Returns true if |binding_| is set.
  bool IsConnected() const { return binding_.get(); }

  // Sets (or resets if |host| is nullptr) the host.
  void SetHost(HostType* host) {
    // Some tests overwrite host, now. It is safe iff the |instance_| is
    // not yet set.
    // TODO(hidehiko): Make check more strict.
    DCHECK(host == nullptr || host_ == nullptr || instance_ == nullptr);

    if (host_ == host)
      return;

    host_ = host;
    OnChanged();
  }

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
    OnChanged();
  }

 private:
  // Called when |instance_| or |host_| is updated from null to non-null or
  // from non-null to null.
  void OnChanged() {
    if (instance_ && host_) {
      // When both get ready, start connection.
      // TODO(crbug.com/750563): Fix the race issue.
      binding_ = std::make_unique<mojo::Binding<HostType>>(host_);
      mojo::InterfacePtr<HostType> host_proxy;
      binding_->Bind(mojo::MakeRequest(&host_proxy));
      // Note: because the callback will be destroyed with |binding_|,
      // base::Unretained() can be safely used.
      binding_->set_connection_error_handler(base::BindOnce(
          &mojo::Binding<HostType>::Close, base::Unretained(binding_.get())));

      // Call the appropriate version of Init().
      CallInstanceInit<InstanceType>(std::move(host_proxy),
                                     HasInitDeprecated<InstanceType>());
    } else if (binding_.get()) {
      // Otherwise, the connection is closed. If it was connected,
      // reset the host binding and notify.
      binding_.reset();
      connection_notifier_->NotifyConnectionClosed();
    }
  }

  // Dispatches the correct version of Init(). The template type is needed
  // because std::enable_if<> needs to depend on a template parameter in order
  // for SFINAE to work. The second parameter (std::true_type or
  // std::false_type) refers to whether InstanceType::DeprecatedInit() exists.
  template <class T>
  typename std::enable_if<CountInitArgs<T>::value == 2, void>::type
  CallInstanceInit(mojo::InterfacePtr<HostType> host_proxy, std::true_type) {
    if (instance_version_ < InstanceType::kInitMinVersion) {
      // The instance is too old to know about the new Init() version. For now,
      // call the deprecated version for backwards-compatibility.
      // TODO(crbug.com/750563): Deprecate this version.
      CallInstanceInitDeprecated(std::move(host_proxy),
                                 HasInitDeprecated<InstanceType>());
      return;
    }

    instance_->Init(std::move(host_proxy),
                    base::BindOnce(&ConnectionHolderImpl::OnConnectionReady,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  template <class T>
  typename std::enable_if<CountInitArgs<T>::value == 2, void>::type
  CallInstanceInit(mojo::InterfacePtr<HostType> host_proxy, std::false_type) {
    instance_->Init(std::move(host_proxy),
                    base::BindOnce(&ConnectionHolderImpl::OnConnectionReady,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(crbug.com/750563): Deprecate this version.
  template <class T>
  typename std::enable_if<CountInitArgs<T>::value == 1, void>::type
  CallInstanceInit(mojo::InterfacePtr<HostType> host_proxy, ...) {
    instance_->Init(std::move(host_proxy));
    OnConnectionReady();
  }

  void CallInstanceInitDeprecated(mojo::InterfacePtr<HostType> host_proxy,
                                  std::true_type) {
    instance_->InitDeprecated(std::move(host_proxy));
    OnConnectionReady();
  }

  void CallInstanceInitDeprecated(mojo::InterfacePtr<HostType> host_proxy,
                                  std::false_type) {
    // If InitDeprecated does not exists, ARC container must support
    // Init() with callback, already. Thus, this should not be called.
    NOTREACHED();
  }

  // Notifies all the observers that the connection is ready.
  void OnConnectionReady() { connection_notifier_->NotifyConnectionReady(); }

  // This class does not have ownership. The pointers should be managed by the
  // caller.
  ConnectionNotifier* const connection_notifier_;
  InstanceType* instance_ = nullptr;
  uint32_t instance_version_ = 0;
  HostType* host_ = nullptr;

  // Created when both |instance_| and |host_| ptr are set.
  std::unique_ptr<mojo::Binding<HostType>> binding_;

  base::WeakPtrFactory<ConnectionHolderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionHolderImpl);
};

// Single direction Mojo connection holder implementation.
// (HostType == void means single direction).
// InstanceType should be Mojo interface type (arc::mojom::XxxInstance).
template <typename InstanceType>
class ConnectionHolderImpl<InstanceType, void> {
 public:
  // InstanceType must not have Init() method, which should be for a
  // full-duplex connection.
  static_assert(!HasInit<InstanceType>::value,
                "Full duplex ConnectionHolderImpl should be used instead");

  explicit ConnectionHolderImpl(ConnectionNotifier* connection_notifier)
      : connection_notifier_(connection_notifier) {}

  InstanceType* instance() { return instance_; }
  uint32_t instance_version() const { return instance_version_; }

  // For single direction connection, when Instance proxy is obtained,
  // it means connected.
  bool IsConnected() const { return instance_; }

  void SetHost(void* unused) {
    static_assert(!sizeof(*this),
                  "ConnectionHolder::SetHost for single direction connection "
                  "is called unexpectedly.");
    NOTREACHED();
  }

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

    // There is nothing more than setting in this case. Notify observers.
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

// Holds a Mojo connection. This also allows for listening for state changes
// for the particular connection.
// InstanceType and HostType should be Mojo interface type
// (arc::mojom::XxxInstance and arc::mojom::XxxHost respectively).
// If HostType is void, it is single direction Mojo connection, so it only
// looks at Instance pointer.
// Otherwise, it is full duplex Mojo connection. When both Instance and Host
// pointers are set, it calls Instance::Init() method to pass Host pointer
// to the ARC container.
template <typename InstanceType, typename HostType = void>
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
  // |min_version|. Returns nullptr if the connection is either not ready or
  // the instance does not have the requested version, and logs appropriately.
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

  // Sets |host|. This can be called in both cases; on ready, or on closed.
  // Passing nullptr to |host| means closing.
  // This must not be called if HostType is void (i.e. single direciton
  // connection).
  void SetHost(HostType* host) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    impl_.SetHost(host);
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
  internal::ConnectionHolderImpl<InstanceType, HostType> impl_{
      &connection_notifier_};

  DISALLOW_COPY_AND_ASSIGN(ConnectionHolder);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_CONNECTION_HOLDER_H_
