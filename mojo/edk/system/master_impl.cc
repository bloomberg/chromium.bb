// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/master_impl.h"

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "mojo/edk/embedder/embedder.h"

#if defined(OS_WIN)
base::LazyInstance<base::hash_map<uint64_t, HANDLE>>::Leaky
    g_token_map = LAZY_INSTANCE_INITIALIZER;
#endif

namespace mojo {
namespace edk {

MasterImpl::MasterImpl(base::ProcessId slave_pid)
#if defined(OS_WIN)
    : slave_process_(
          base::Process::OpenWithAccess(slave_pid, PROCESS_DUP_HANDLE))
#endif
      {
#if defined(OS_WIN)
  DCHECK(slave_process_.IsValid());
#endif
}

MasterImpl::~MasterImpl() {
}

void MasterImpl::HandleToToken(ScopedHandle platform_handle,
                               const HandleToTokenCallback& callback) {
#if defined(OS_WIN)
  ScopedPlatformHandle sender_handle;
  MojoResult unwrap_result = PassWrappedPlatformHandle(
      platform_handle.get().value(), &sender_handle);
  if (unwrap_result != MOJO_RESULT_OK) {
    DLOG(WARNING) << "HandleToToken couldn't unwrap platform handle.";
    callback.Run(unwrap_result, 0);
    return;
  }

  HANDLE master_handle = NULL;
  BOOL dup_result =
      DuplicateHandle(slave_process_.Handle(), sender_handle.release().handle,
                      base::GetCurrentProcessHandle(), &master_handle, 0,
                      FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  if (!dup_result) {
    DLOG(WARNING) << "HandleToToken couldn't duplicate slave handle.";
    callback.Run(MOJO_RESULT_INVALID_ARGUMENT, 0);
    return;
  }

  uint64_t token = base::RandUint64();
  g_token_map.Get()[token] = master_handle;
  callback.Run(MOJO_RESULT_OK, token);
#else
  NOTREACHED();
#endif
}

void MasterImpl::TokenToHandle(uint64_t token,
                               const TokenToHandleCallback& callback) {
#if defined(OS_WIN)
  auto it = g_token_map.Get().find(token);
  if (it == g_token_map.Get().end()) {
    DLOG(WARNING) << "TokenToHandle didn't find token.";
    callback.Run(MOJO_RESULT_INVALID_ARGUMENT, ScopedHandle());
    return;
  }

  HANDLE slave_handle = NULL;
  BOOL dup_result =
      DuplicateHandle(base::GetCurrentProcessHandle(), it->second,
                      slave_process_.Handle(), &slave_handle, 0,
                      FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  if (!dup_result) {
    DLOG(WARNING) << "TokenToHandle couldn't duplicate slave handle.";
    callback.Run(MOJO_RESULT_INTERNAL, ScopedHandle());
    return;
  }

  ScopedPlatformHandle platform_handle;
  platform_handle.reset(PlatformHandle(slave_handle));

  MojoHandle handle;
  MojoResult wrap_result = CreatePlatformHandleWrapper(
      platform_handle.Pass(), &handle);
  if (wrap_result != MOJO_RESULT_OK) {
    DLOG(WARNING) << "TokenToHandle couldn't unwrap platform handle.";
    callback.Run(wrap_result, ScopedHandle());
    return;
  }

  callback.Run(MOJO_RESULT_OK, ScopedHandle(Handle(handle)));
  g_token_map.Get().erase(it);
#else
  NOTREACHED();
#endif
}

}  // namespace edk
}  // namespace mojo
