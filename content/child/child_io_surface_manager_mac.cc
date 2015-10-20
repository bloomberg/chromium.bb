// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_io_surface_manager_mac.h"

#include "base/mac/mach_logging.h"
#include "content/common/mac/io_surface_manager_messages.h"

namespace content {

// static
ChildIOSurfaceManager* ChildIOSurfaceManager::GetInstance() {
  return base::Singleton<
      ChildIOSurfaceManager,
      base::LeakySingletonTraits<ChildIOSurfaceManager>>::get();
}

bool ChildIOSurfaceManager::RegisterIOSurface(IOSurfaceId io_surface_id,
                                              int client_id,
                                              IOSurfaceRef io_surface) {
  DCHECK(service_port_.is_valid());
  DCHECK(!token_.IsZero());

  mach_port_t reply_port;
  kern_return_t kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE, &reply_port);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_allocate";
    return false;
  }
  base::mac::ScopedMachReceiveRight scoped_receive_right(reply_port);

  // Deallocate the right after sending a copy to the parent.
  base::mac::ScopedMachSendRight scoped_io_surface_right(
      IOSurfaceCreateMachPort(io_surface));

  union {
    IOSurfaceManagerHostMsg_RegisterIOSurface request;
    struct {
      IOSurfaceManagerMsg_RegisterIOSurfaceReply msg;
      mach_msg_trailer_t trailer;
    } reply;
  } data = {{{0}}};
  data.request.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) |
      MACH_MSGH_BITS_COMPLEX;
  data.request.header.msgh_remote_port = service_port_.get();
  data.request.header.msgh_local_port = reply_port;
  data.request.header.msgh_size = sizeof(data.request);
  data.request.header.msgh_id = IOSurfaceManagerHostMsg_RegisterIOSurface::ID;
  data.request.body.msgh_descriptor_count = 1;
  data.request.io_surface_port.name = scoped_io_surface_right.get();
  data.request.io_surface_port.disposition = MACH_MSG_TYPE_COPY_SEND;
  data.request.io_surface_port.type = MACH_MSG_PORT_DESCRIPTOR;
  data.request.io_surface_id = io_surface_id.id;
  data.request.client_id = client_id;
  memcpy(data.request.token_name, token_.name, sizeof(token_.name));

  kr = mach_msg(&data.request.header, MACH_SEND_MSG | MACH_RCV_MSG,
                sizeof(data.request), sizeof(data.reply), reply_port,
                MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return false;
  }

  return data.reply.msg.result;
}

void ChildIOSurfaceManager::UnregisterIOSurface(IOSurfaceId io_surface_id,
                                                int client_id) {
  DCHECK(service_port_.is_valid());
  DCHECK(!token_.IsZero());

  IOSurfaceManagerHostMsg_UnregisterIOSurface request = {{0}};
  request.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  request.header.msgh_remote_port = service_port_.get();
  request.header.msgh_local_port = MACH_PORT_NULL;
  request.header.msgh_size = sizeof(request);
  request.header.msgh_id = IOSurfaceManagerHostMsg_UnregisterIOSurface::ID;
  request.io_surface_id = io_surface_id.id;
  request.client_id = client_id;
  memcpy(request.token_name, token_.name, sizeof(token_.name));

  kern_return_t kr =
      mach_msg(&request.header, MACH_SEND_MSG, sizeof(request), 0,
               MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
  }
}

IOSurfaceRef ChildIOSurfaceManager::AcquireIOSurface(
    IOSurfaceId io_surface_id) {
  DCHECK(service_port_.is_valid());
  DCHECK(!token_.IsZero());

  mach_port_t reply_port;
  kern_return_t kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE, &reply_port);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_allocate";
    return nullptr;
  }
  base::mac::ScopedMachReceiveRight scoped_receive_right(reply_port);

  union {
    IOSurfaceManagerHostMsg_AcquireIOSurface request;
    struct {
      IOSurfaceManagerMsg_AcquireIOSurfaceReply msg;
      mach_msg_trailer_t trailer;
    } reply;
  } data = {{{0}}};
  data.request.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
  data.request.header.msgh_remote_port = service_port_.get();
  data.request.header.msgh_local_port = reply_port;
  data.request.header.msgh_size = sizeof(data.request);
  data.request.header.msgh_id = IOSurfaceManagerHostMsg_AcquireIOSurface::ID;
  data.request.io_surface_id = io_surface_id.id;
  memcpy(data.request.token_name, token_.name, sizeof(token_.name));

  kr = mach_msg(&data.request.header, MACH_SEND_MSG | MACH_RCV_MSG,
                sizeof(data.request), sizeof(data.reply), reply_port,
                MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return nullptr;
  }
  if (!data.reply.msg.result) {
    DLOG(ERROR) << "Browser refused AcquireIOSurface request";
    return nullptr;
  }

  // Deallocate the right after creating an IOSurface reference.
  base::mac::ScopedMachSendRight scoped_io_surface_right(
      data.reply.msg.io_surface_port.name);

  return IOSurfaceLookupFromMachPort(scoped_io_surface_right.get());
}

ChildIOSurfaceManager::ChildIOSurfaceManager() {}

ChildIOSurfaceManager::~ChildIOSurfaceManager() {}

}  // namespace content
