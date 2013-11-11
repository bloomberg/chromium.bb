// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/system/core.h"

#include "base/logging.h"
#include "mojo/system/core_impl.h"

extern "C" {

MojoResult MojoClose(MojoHandle handle) {
  DCHECK(mojo::system::CoreImpl::Get()) << "CoreImpl not initialized.";
  return mojo::system::CoreImpl::Get()->Close(handle);
}

MojoResult MojoWait(MojoHandle handle,
                    MojoWaitFlags flags,
                    MojoDeadline deadline) {
  DCHECK(mojo::system::CoreImpl::Get()) << "CoreImpl not initialized.";
  return mojo::system::CoreImpl::Get()->Wait(handle, flags, deadline);
}

MojoResult MojoWaitMany(const MojoHandle* handles,
                        const MojoWaitFlags* flags,
                        uint32_t num_handles,
                        MojoDeadline deadline) {
  DCHECK(mojo::system::CoreImpl::Get()) << "CoreImpl not initialized.";
  return mojo::system::CoreImpl::Get()->WaitMany(handles, flags, num_handles,
                                                 deadline);
}

MojoResult MojoCreateMessagePipe(MojoHandle* handle_0, MojoHandle* handle_1) {
  DCHECK(mojo::system::CoreImpl::Get()) << "CoreImpl not initialized.";
  return mojo::system::CoreImpl::Get()->CreateMessagePipe(handle_0, handle_1);
}

MojoResult MojoWriteMessage(MojoHandle handle,
                            const void* bytes, uint32_t num_bytes,
                            const MojoHandle* handles, uint32_t num_handles,
                            MojoWriteMessageFlags flags) {
  DCHECK(mojo::system::CoreImpl::Get()) << "CoreImpl not initialized.";
  return mojo::system::CoreImpl::Get()->WriteMessage(handle,
                                                     bytes, num_bytes,
                                                     handles, num_handles,
                                                     flags);
}

MojoResult MojoReadMessage(MojoHandle handle,
                           void* bytes, uint32_t* num_bytes,
                           MojoHandle* handles, uint32_t* num_handles,
                           MojoReadMessageFlags flags) {
  DCHECK(mojo::system::CoreImpl::Get()) << "CoreImpl not initialized.";
  return mojo::system::CoreImpl::Get()->ReadMessage(handle,
                                                    bytes, num_bytes,
                                                    handles, num_handles,
                                                    flags);
}

}  // extern "C"
