// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/scoped_interface_endpoint_handle.h"

namespace mojo {

namespace internal {
class AssociatedInterfacePtrInfoHelper;
}

// AssociatedInterfacePtrInfo stores necessary information to construct an
// associated interface pointer. It is similar to InterfacePtrInfo except that
// it doesn't own a message pipe handle.
template <typename Interface>
class AssociatedInterfacePtrInfo {
  DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND(AssociatedInterfacePtrInfo);

 public:
  AssociatedInterfacePtrInfo() : version_(0u) {}

  AssociatedInterfacePtrInfo(AssociatedInterfacePtrInfo&& other)
      : handle_(std::move(other.handle_)), version_(other.version_) {
    other.version_ = 0u;
  }

  ~AssociatedInterfacePtrInfo() {}

  AssociatedInterfacePtrInfo& operator=(AssociatedInterfacePtrInfo&& other) {
    if (this != &other) {
      handle_ = std::move(other.handle_);
      version_ = other.version_;
      other.version_ = 0u;
    }

    return *this;
  }

  bool is_valid() const { return handle_.is_valid(); }

  uint32_t version() const { return version_; }
  void set_version(uint32_t version) { version_ = version; }

 private:
  friend class internal::AssociatedInterfacePtrInfoHelper;

  internal::ScopedInterfaceEndpointHandle handle_;
  uint32_t version_;
};

namespace internal {

// With this helper, AssociatedInterfacePtrInfo doesn't have to expose any
// operations related to ScopedInterfaceEndpointHandle, which is an internal
// class.
class AssociatedInterfacePtrInfoHelper {
 public:
  template <typename Interface>
  static ScopedInterfaceEndpointHandle PassHandle(
      AssociatedInterfacePtrInfo<Interface>* info) {
    return std::move(info->handle_);
  }

  template <typename Interface>
  static const ScopedInterfaceEndpointHandle& GetHandle(
      AssociatedInterfacePtrInfo<Interface>* info) {
    return info->handle_;
  }

  template <typename Interface>
  static void SetHandle(AssociatedInterfacePtrInfo<Interface>* info,
                        ScopedInterfaceEndpointHandle handle) {
    info->handle_ = std::move(handle);
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_
