// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for screen orientation.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebScreenOrientation.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ScreenOrientationMsgStart

IPC_ENUM_TRAITS(blink::WebScreenOrientation)

// The browser process informs the renderer process that the screen orientation
// has changed. |orientation| contains the new screen orientation in degrees.
IPC_MESSAGE_CONTROL1(ScreenOrientationMsg_OrientationChange,
                     blink::WebScreenOrientation /* orientation */ )
