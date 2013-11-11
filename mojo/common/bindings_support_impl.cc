// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/bindings_support_impl.h"

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "mojo/common/handle_watcher.h"

namespace mojo {
namespace common {

// Context is used to track the number of HandleWatcher objects in use by a
// particular BindingsSupportImpl instance.
class BindingsSupportImpl::Context
    : public base::RefCountedThreadSafe<BindingsSupportImpl::Context> {
 public:
  void CallOnHandleReady(HandleWatcher* watcher,
                         AsyncWaitCallback* callback,
                         MojoResult result) {
    delete watcher;
    callback->OnHandleReady(result);
  }

 private:
  friend class base::RefCountedThreadSafe<Context>;
  virtual ~Context() {}
};

BindingsSupportImpl::BindingsSupportImpl()
    : context_(new Context()) {
}

BindingsSupportImpl::~BindingsSupportImpl() {
  // All HandleWatcher instances created through this interface should have
  // been destroyed.
  DCHECK(context_->HasOneRef());
}

BindingsSupport::AsyncWaitID BindingsSupportImpl::AsyncWait(
    Handle handle,
    MojoWaitFlags flags,
    AsyncWaitCallback* callback) {
  // This instance will be deleted when done or cancelled.
  HandleWatcher* watcher = new HandleWatcher();

  // TODO(darin): Standardize on mojo::Handle instead of MojoHandle?
  watcher->Start(handle.value,
                 flags,
                 MOJO_DEADLINE_INDEFINITE,
                 base::Bind(&Context::CallOnHandleReady,
                            context_,
                            watcher,
                            callback));
  return watcher;
}

void BindingsSupportImpl::CancelWait(AsyncWaitID async_wait_id) {
  HandleWatcher* watcher = static_cast<HandleWatcher*>(async_wait_id);
  delete watcher;
}

}  // namespace common
}  // namespace mojo
