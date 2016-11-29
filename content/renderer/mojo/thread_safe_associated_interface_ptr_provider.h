// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_THREAD_SAFE_ASSOCIATED_INTERFACE_PTR_PROVIDER_H_
#define CONTENT_RENDERER_MOJO_THREAD_SAFE_ASSOCIATED_INTERFACE_PTR_PROVIDER_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/thread_safe_interface_ptr.h"

namespace content {

// This class provides a way to create ThreadSafeAssociatedInterfacePtr's from
// the main thread that can be used right away (even though the backing
// AssociatedInterfacePtr is created on the IO thread and the channel may not be
// connected yet).
class ThreadSafeAssociatedInterfacePtrProvider {
 public:
  // Note that this does not take ownership of |channel_proxy|. It's the
  // caller responsibility to ensure |channel_proxy| outlives this object.
  explicit ThreadSafeAssociatedInterfacePtrProvider(
      IPC::ChannelProxy* channel_proxy)
      : channel_proxy_(channel_proxy) {}

  template <typename Interface>
  scoped_refptr<mojo::ThreadSafeAssociatedInterfacePtr<Interface>>
  CreateInterfacePtr() {
    scoped_refptr<mojo::ThreadSafeAssociatedInterfacePtr<Interface>> ptr =
        mojo::ThreadSafeAssociatedInterfacePtr<Interface>::CreateUnbound();
    channel_proxy_->RetrieveAssociatedInterfaceOnIOThread<Interface>(base::Bind(
        &ThreadSafeAssociatedInterfacePtrProvider::BindInterfacePtr<Interface>,
        ptr));
    return ptr;
  }

 private:
  template <typename Interface>
  static void BindInterfacePtr(
      const scoped_refptr<mojo::ThreadSafeAssociatedInterfacePtr<Interface>>&
          ptr,
      mojo::AssociatedInterfacePtr<Interface> interface_ptr) {
    bool success = ptr->Bind(std::move(interface_ptr));
    DCHECK(success);
  }

  IPC::ChannelProxy* channel_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeAssociatedInterfacePtrProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_THREAD_SAFE_ASSOCIATED_INTERFACE_PTR_PROVIDER_H_