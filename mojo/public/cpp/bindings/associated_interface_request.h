// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/scoped_interface_endpoint_handle.h"

namespace mojo {

namespace internal {
class AssociatedInterfaceRequestHelper;
}

// AssociatedInterfaceRequest represents an associated interface request. It is
// similar to InterfaceRequest except that it doesn't own a message pipe handle.
template <typename Interface>
class AssociatedInterfaceRequest {
  DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND(AssociatedInterfaceRequest);

 public:
  // Constructs an empty AssociatedInterfaceRequest, representing that the
  // client is not requesting an implementation of Interface.
  AssociatedInterfaceRequest() {}
  AssociatedInterfaceRequest(decltype(nullptr)) {}

  // Takes the interface endpoint handle from another
  // AssociatedInterfaceRequest.
  AssociatedInterfaceRequest(AssociatedInterfaceRequest&& other) {
    handle_ = std::move(other.handle_);
  }
  AssociatedInterfaceRequest& operator=(AssociatedInterfaceRequest&& other) {
    if (this != &other)
      handle_ = std::move(other.handle_);
    return *this;
  }

  // Assigning to nullptr resets the AssociatedInterfaceRequest to an empty
  // state, closing the interface endpoint handle currently bound to it (if
  // any).
  AssociatedInterfaceRequest& operator=(decltype(nullptr)) {
    handle_.reset();
    return *this;
  }

  // Indicates whether the request currently contains a valid interface endpoint
  // handle.
  bool is_pending() const { return handle_.is_valid(); }

 private:
  friend class internal::AssociatedInterfaceRequestHelper;

  internal::ScopedInterfaceEndpointHandle handle_;
};

namespace internal {

// With this helper, AssociatedInterfaceRequest doesn't have to expose any
// operations related to ScopedInterfaceEndpointHandle, which is an internal
// class.
class AssociatedInterfaceRequestHelper {
 public:
  template <typename Interface>
  static ScopedInterfaceEndpointHandle PassHandle(
      AssociatedInterfaceRequest<Interface>* request) {
    return std::move(request->handle_);
  }

  template <typename Interface>
  static const ScopedInterfaceEndpointHandle& GetHandle(
      AssociatedInterfaceRequest<Interface>* request) {
    return request->handle_;
  }

  template <typename Interface>
  static void SetHandle(AssociatedInterfaceRequest<Interface>* request,
                        ScopedInterfaceEndpointHandle handle) {
    request->handle_ = std::move(handle);
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_
