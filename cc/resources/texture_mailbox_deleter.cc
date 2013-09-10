// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/texture_mailbox_deleter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/output/context_provider.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"

namespace cc {

static void DeleteTextureOnImplThread(
    const scoped_refptr<ContextProvider>& context_provider,
    unsigned texture_id,
    unsigned sync_point,
    bool is_lost) {
  if (sync_point)
    context_provider->Context3d()->waitSyncPoint(sync_point);
  context_provider->Context3d()->deleteTexture(texture_id);
}

static void PostTaskFromMainToImplThread(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    TextureMailbox::ReleaseCallback run_impl_callback,
    unsigned sync_point,
    bool is_lost) {
  // This posts the task to RunDeleteTextureOnImplThread().
  impl_task_runner->PostTask(
      FROM_HERE, base::Bind(run_impl_callback, sync_point, is_lost));
}

TextureMailboxDeleter::TextureMailboxDeleter() : weak_ptr_factory_(this) {}

TextureMailboxDeleter::~TextureMailboxDeleter() {
  for (size_t i = 0; i < impl_callbacks_.size(); ++i)
    impl_callbacks_.at(i)->Run(0, true);
}

TextureMailbox::ReleaseCallback TextureMailboxDeleter::GetReleaseCallback(
    const scoped_refptr<ContextProvider>& context_provider,
    unsigned texture_id) {
  // This callback owns a reference on the |context_provider|. It must be
  // destroyed on the impl thread. Upon destruction of this class, the
  // callback must immediately be destroyed.
  scoped_ptr<TextureMailbox::ReleaseCallback> impl_callback(
      new TextureMailbox::ReleaseCallback(base::Bind(
          &DeleteTextureOnImplThread, context_provider, texture_id)));

  impl_callbacks_.push_back(impl_callback.Pass());

  // The raw pointer to the impl-side callback is valid as long as this
  // class is alive. So we guard it with a WeakPtr.
  TextureMailbox::ReleaseCallback run_impl_callback =
      base::Bind(&TextureMailboxDeleter::RunDeleteTextureOnImplThread,
                 weak_ptr_factory_.GetWeakPtr(),
                 impl_callbacks_.back());

  // Provide a callback for the main thread that posts back to the impl
  // thread.
  TextureMailbox::ReleaseCallback main_callback =
      base::Bind(&PostTaskFromMainToImplThread,
                 base::MessageLoopProxy::current(),
                 run_impl_callback);

  return main_callback;
}

void TextureMailboxDeleter::RunDeleteTextureOnImplThread(
    TextureMailbox::ReleaseCallback* impl_callback,
    unsigned sync_point,
    bool is_lost) {
  for (size_t i = 0; i < impl_callbacks_.size(); ++i) {
    if (impl_callbacks_.at(i)->Equals(*impl_callback)) {
      // Run the callback, then destroy it here on the impl thread.
      impl_callback->Run(sync_point, is_lost);
      impl_callbacks_.erase(impl_callbacks_.begin() + i);
      return;
    }
  }

  NOTREACHED() << "The Callback returned by GetDeleteCallback() was called "
               << "more than once.";
}

}  // namespace cc
