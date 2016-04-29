// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/gpu_type_converters.h"

#include "ipc/ipc_channel_handle.h"
#include "mojo/platform_handle/platform_handle_functions.h"

namespace mojo {

// static
mus::mojom::ChannelHandlePtr
TypeConverter<mus::mojom::ChannelHandlePtr, IPC::ChannelHandle>::Convert(
    const IPC::ChannelHandle& handle) {
  mus::mojom::ChannelHandlePtr result = mus::mojom::ChannelHandle::New();
  result->name = handle.name;
#if defined(OS_WIN)
  // On windows, a pipe handle Will NOT be marshalled over IPC.
  DCHECK(handle.pipe.handle == NULL);
#else
  MojoPlatformHandle platform_handle =
      handle.socket.fd == -1 ? -1 : dup(handle.socket.fd);
  MojoHandle mojo_handle = MOJO_HANDLE_INVALID;
  if (platform_handle != -1) {
    MojoResult create_result =
        MojoCreatePlatformHandleWrapper(platform_handle, &mojo_handle);
    if (create_result == MOJO_RESULT_OK)
      result->socket.reset(mojo::Handle(mojo_handle));
  }
#endif
  return result;
}

// static
IPC::ChannelHandle
TypeConverter<IPC::ChannelHandle, mus::mojom::ChannelHandlePtr>::Convert(
    const mus::mojom::ChannelHandlePtr& handle) {
  if (handle.is_null())
    return IPC::ChannelHandle();
#if defined(OS_WIN)
  // On windows, a pipe handle Will NOT be marshalled over IPC.
  DCHECK(!handle->socket.is_valid());
  return IPC::ChannelHandle(handle->name);
#else
  MojoPlatformHandle platform_handle = -1;
  MojoExtractPlatformHandle(handle->socket.release().value(), &platform_handle);
  return IPC::ChannelHandle(handle->name,
                            base::FileDescriptor(platform_handle, true));
#endif
}

}  // namespace mojo
