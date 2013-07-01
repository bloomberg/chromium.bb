// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for requesting WebRTC identity.
// Multiply-included message file, hence no include guard.

#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START WebRTCIdentityMsgStart

// Messages sent from the renderer to the browser.
// Request a WebRTC identity.
IPC_MESSAGE_CONTROL4(WebRTCIdentityMsg_RequestIdentity,
                     int /* request_id */,
                     GURL /* origin */,
                     std::string /* identity_name */,
                     std::string /* common_name */)
// Cancel a WebRTC identity request.
IPC_MESSAGE_CONTROL1(WebRTCIdentityMsg_CancelRequest, int /* request_id */)

// Messages sent from the browser to the renderer.
// Return a WebRTC identity.
IPC_MESSAGE_CONTROL3(WebRTCIdentityHostMsg_IdentityReady,
                     int /* request_id */,
                     std::string /* certificate */,
                     std::string /* private_key */)
// Notifies an error from the identity request.
IPC_MESSAGE_CONTROL2(WebRTCIdentityHostMsg_RequestFailed,
                     int /* request_id */,
                     int /* error */)
