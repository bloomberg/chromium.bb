// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

class AssociatedGroupController;

// AssociatedGroup refers to all the interface endpoints running at one end of a
// message pipe.
// It is thread safe and cheap to make copies.
class MOJO_CPP_BINDINGS_EXPORT AssociatedGroup {
 public:
  AssociatedGroup();

  explicit AssociatedGroup(scoped_refptr<AssociatedGroupController> controller);

  explicit AssociatedGroup(const ScopedInterfaceEndpointHandle& handle);

  AssociatedGroup(const AssociatedGroup& other);

  ~AssociatedGroup();

  AssociatedGroup& operator=(const AssociatedGroup& other);

  // The return value of this getter if this object is initialized with a
  // ScopedInterfaceEndpointHandle:
  //   - If the handle is invalid, the return value will always be null.
  //   - If the handle is valid and non-pending, the return value will be
  //     non-null and remain unchanged even if the handle is later reset.
  //   - If the handle is pending asssociation, the return value will initially
  //     be null, change to non-null when/if the handle is associated, and
  //     remain unchanged ever since.
  AssociatedGroupController* GetController();

  // TODO(yzshen): Remove the following public method. It is not needed anymore.
  // Configuration used by CreateAssociatedInterface(). Please see the comments
  // of that method for more details.
  enum AssociatedInterfaceConfig { WILL_PASS_PTR, WILL_PASS_REQUEST };

  // |config| indicates whether |ptr_info| or |request| will be sent to the
  // remote side of the message pipe.
  //
  // NOTE: If |config| is |WILL_PASS_REQUEST|, you will want to bind |ptr_info|
  // to a local AssociatedInterfacePtr to make calls. However, there is one
  // restriction: the pointer should NOT be used to make calls before |request|
  // is sent. Violating that will cause the message pipe to be closed. On the
  // other hand, as soon as |request| is sent, the pointer is usable. There is
  // no need to wait until |request| is bound to an implementation at the remote
  // side.
  template <typename T>
  void CreateAssociatedInterface(AssociatedInterfaceConfig config,
                                 AssociatedInterfacePtrInfo<T>* ptr_info,
                                 AssociatedInterfaceRequest<T>* request) {
    ScopedInterfaceEndpointHandle handle0;
    ScopedInterfaceEndpointHandle handle1;
    ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(&handle0,
                                                                &handle1);

    ptr_info->set_handle(std::move(handle0));
    request->Bind(std::move(handle1));

    if (config == WILL_PASS_PTR) {
      // The implementation is local, therefore set the version according to
      // the interface definition that this code is built against.
      ptr_info->set_version(T::Version_);
    } else {
      // The implementation is remote, we don't know about its actual version
      // yet.
      ptr_info->set_version(0u);
    }
  }

 private:
  base::Callback<AssociatedGroupController*()> controller_getter_;
  scoped_refptr<AssociatedGroupController> controller_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_
