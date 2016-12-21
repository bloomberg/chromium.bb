// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for screen orientation.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationError.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ScreenOrientationMsgStart

// Only used by content/common/manifest_manager_messages.h.
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebScreenOrientationLockType,
                              blink::WebScreenOrientationLockDefault,
                              blink::WebScreenOrientationLockNatural)
