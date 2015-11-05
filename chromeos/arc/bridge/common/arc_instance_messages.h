// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages sent from the ARC instance to the host
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include "base/file_descriptor_posix.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ArcInstanceMsgStart

// Registers a virtual input device on the container side.
// |name| is the device name, like "Chrome OS Keyboard".
// |device_type| is the device type, like "keyboard".
// The virtual device will be reading 'struct input_event's from |fd|.  The
// ownership of |fd| will be transferred to the receiver, so the sender must
// not close it.
IPC_MESSAGE_CONTROL3(ArcInstanceMsg_RegisterInputDevice,
                     std::string, /* name */
                     std::string, /* device_type */
                     base::FileDescriptor /* fd */)
