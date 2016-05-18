// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SCOPED_INTERFACE_ENDPOINT_HANDLE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SCOPED_INTERFACE_ENDPOINT_HANDLE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/move.h"
#include "mojo/public/cpp/bindings/lib/interface_id.h"

namespace mojo {

namespace internal {
class MultiplexRouter;
}

// ScopedInterfaceEndpointHandle refers to one end of an interface, either the
// implementation side or the client side.
class ScopedInterfaceEndpointHandle {
  DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND(ScopedInterfaceEndpointHandle);

 public:
  // Creates an invalid endpoint handle.
  ScopedInterfaceEndpointHandle();

  ScopedInterfaceEndpointHandle(ScopedInterfaceEndpointHandle&& other);

  ~ScopedInterfaceEndpointHandle();

  ScopedInterfaceEndpointHandle& operator=(
      ScopedInterfaceEndpointHandle&& other);

  bool is_valid() const { return internal::IsValidInterfaceId(id_); }

  bool is_local() const { return is_local_; }

  void reset();
  void swap(ScopedInterfaceEndpointHandle& other);

  // DO NOT USE METHODS BELOW THIS LINE. These are for internal use and testing.

  internal::InterfaceId id() const { return id_; }

  internal::MultiplexRouter* router() const { return router_.get(); }

  // Releases the handle without closing it.
  internal::InterfaceId release();

 private:
  friend class internal::MultiplexRouter;

  // This is supposed to be used by MultiplexRouter only.
  // |id| is the corresponding interface ID.
  // If |is_local| is false, this handle is meant to be passed over |router| to
  // the remote side.
  ScopedInterfaceEndpointHandle(
      internal::InterfaceId id,
      bool is_local,
      scoped_refptr<internal::MultiplexRouter> router);

  internal::InterfaceId id_;
  bool is_local_;
  scoped_refptr<internal::MultiplexRouter> router_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SCOPED_INTERFACE_ENDPOINT_HANDLE_H_
