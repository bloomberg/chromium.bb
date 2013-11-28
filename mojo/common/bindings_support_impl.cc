// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/bindings_support_impl.h"

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "mojo/common/handle_watcher.h"

namespace mojo {
namespace common {
namespace {
base::LazyInstance<base::ThreadLocalPointer<Buffer> >::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;
}

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

Buffer* BindingsSupportImpl::GetCurrentBuffer() {
  return lazy_tls_ptr.Pointer()->Get();
}

Buffer* BindingsSupportImpl::SetCurrentBuffer(Buffer* buf) {
  Buffer* old_buf = lazy_tls_ptr.Pointer()->Get();
  lazy_tls_ptr.Pointer()->Set(buf);
  return old_buf;
}

BindingsSupport::AsyncWaitID BindingsSupportImpl::AsyncWait(
    const Handle& handle,
    MojoWaitFlags flags,
    AsyncWaitCallback* callback) {
  // This instance will be deleted when done or cancelled.
  HandleWatcher* watcher = new HandleWatcher();

  watcher->Start(handle,
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
