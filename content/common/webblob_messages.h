// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for HTML5 Blob.
// Multiply-included message file, hence no include guard.

#include "content/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START BlobMsgStart

// Blob messages sent from the renderer to the browser.

// Registers a blob URL referring to the specified blob data.
IPC_MESSAGE_CONTROL2(BlobHostMsg_RegisterBlobUrl,
                     GURL /* url */,
                     scoped_refptr<webkit_blob::BlobData> /* blob_data */)

// Registers a blob URL referring to the blob data identified by the specified
// source URL.
IPC_MESSAGE_CONTROL2(BlobHostMsg_RegisterBlobUrlFrom,
                     GURL /* url */,
                     GURL /* src_url */)

// Unregister a blob URL.
IPC_MESSAGE_CONTROL1(BlobHostMsg_UnregisterBlobUrl,
                     GURL /* url */)
