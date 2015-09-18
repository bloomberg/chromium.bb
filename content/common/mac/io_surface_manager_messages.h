// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_MESSAGES_H_
#define CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_MESSAGES_H_

#include <mach/message.h>

#include "content/common/mac/io_surface_manager_token.h"

// Mach messages used for child processes.

// Message IDs.
// Note: we currently use __LINE__ to give unique IDs to messages.
// They're unique since all messages are defined in this file.
#define MACH_MESSAGE_ID() __LINE__

namespace content {

// Messages sent from the child process to the browser.

struct IOSurfaceManagerHostMsg_RegisterIOSurface {
  enum { ID = MACH_MESSAGE_ID() };
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t io_surface_port;
  IOSurfaceManagerToken::Name token_name;
  int io_surface_id;
  int client_id;
};

struct IOSurfaceManagerHostMsg_UnregisterIOSurface {
  enum { ID = MACH_MESSAGE_ID() };
  mach_msg_header_t header;
  mach_msg_body_t body;
  IOSurfaceManagerToken::Name token_name;
  int io_surface_id;
  int client_id;
};

struct IOSurfaceManagerHostMsg_AcquireIOSurface {
  enum { ID = MACH_MESSAGE_ID() };
  mach_msg_header_t header;
  mach_msg_body_t body;
  IOSurfaceManagerToken::Name token_name;
  int io_surface_id;
};

// Messages sent from the browser to the child process.

struct IOSurfaceManagerMsg_RegisterIOSurfaceReply {
  mach_msg_header_t header;
  mach_msg_body_t body;
  boolean_t result;
};

struct IOSurfaceManagerMsg_AcquireIOSurfaceReply {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t io_surface_port;
  boolean_t result;
};

}  // namespace content

#endif  // CONTENT_COMMON_MAC_IO_SURFACE_MANAGER_MESSAGES_H_
