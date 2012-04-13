// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "content/public/common/common_param_traits.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"
#include "webkit/dom_storage/dom_storage_types.h"

#define IPC_MESSAGE_START DOMStorageMsgStart

// Signals a storage event.
IPC_STRUCT_BEGIN(DOMStorageMsg_Event_Params)
  // The key that generated the storage event.  Null if clear() was called.
  IPC_STRUCT_MEMBER(NullableString16, key)

  // The old value of this key.  Null on clear() or if it didn't have a value.
  IPC_STRUCT_MEMBER(NullableString16, old_value)

  // The new value of this key.  Null on removeItem() or clear().
  IPC_STRUCT_MEMBER(NullableString16, new_value)

  // The origin this is associated with.
  IPC_STRUCT_MEMBER(string16, origin)

  // The URL of the page that caused the storage event.
  IPC_STRUCT_MEMBER(GURL, page_url)

  // The namespace_id this is associated with.
  IPC_STRUCT_MEMBER(int64, namespace_id)
IPC_STRUCT_END()

IPC_ENUM_TRAITS(WebKit::WebStorageArea::Result)

// DOM Storage messages sent from the browser to the renderer.

// Storage events are broadcast to renderer processes.
IPC_MESSAGE_CONTROL1(DOMStorageMsg_Event,
                     DOMStorageMsg_Event_Params)


// DOM Storage messages sent from the renderer to the browser.

// Open the storage area for a particular origin within a namespace.
// TODO(michaeln): make this async and have the renderer send the connection_id
IPC_SYNC_MESSAGE_CONTROL2_1(DOMStorageHostMsg_OpenStorageArea,
                            int64 /* namespace_id */,
                            string16 /* origin */,
                            int64 /* connection_id */)

// Close a previously opened storage area.
IPC_MESSAGE_CONTROL1(DOMStorageHostMsg_CloseStorageArea,
                     int64 /* connection_id */)

// Get the length of a storage area.
IPC_SYNC_MESSAGE_CONTROL1_1(DOMStorageHostMsg_Length,
                            int64 /* connection_id */,
                            unsigned /* length */)

// Get a the ith key within a storage area.
IPC_SYNC_MESSAGE_CONTROL2_1(DOMStorageHostMsg_Key,
                            int64 /* connection_id */,
                            unsigned /* index */,
                            NullableString16 /* key */)

// Get a value based on a key from a storage area.
IPC_SYNC_MESSAGE_CONTROL2_1(DOMStorageHostMsg_GetItem,
                            int64 /* connection_id */,
                            string16 /* key */,
                            NullableString16 /* value */)

// Set a value that's associated with a key in a storage area.
IPC_SYNC_MESSAGE_CONTROL4_2(DOMStorageHostMsg_SetItem,
                            int64 /* connection_id */,
                            string16 /* key */,
                            string16 /* value */,
                            GURL /* page_url */,
                            WebKit::WebStorageArea::Result /* result */,
                            NullableString16 /* old_value */)

// Remove the value associated with a key in a storage area.
IPC_SYNC_MESSAGE_CONTROL3_1(DOMStorageHostMsg_RemoveItem,
                            int64 /* connection_id */,
                            string16 /* key */,
                            GURL /* page_url */,
                            NullableString16 /* old_value */)

// Clear the storage area.
IPC_SYNC_MESSAGE_CONTROL2_1(DOMStorageHostMsg_Clear,
                            int64 /* connection_id */,
                            GURL /* page_url */,
                            bool /* something_cleared */)

