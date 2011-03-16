// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "chrome/common/dom_storage_common.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"

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
  IPC_STRUCT_MEMBER(GURL, url)

  // The storage type of this event.
  IPC_STRUCT_MEMBER(DOMStorageType, storage_type)
IPC_STRUCT_END()

IPC_ENUM_TRAITS(DOMStorageType)
IPC_ENUM_TRAITS(WebKit::WebStorageArea::Result)

// DOM Storage messages sent from the browser to the renderer.

// Storage events are broadcast to renderer processes.
IPC_MESSAGE_CONTROL1(DOMStorageMsg_Event,
                     DOMStorageMsg_Event_Params)


// DOM Storage messages sent from the renderer to the browser.


// Get the storage area id for a particular origin within a namespace.
IPC_SYNC_MESSAGE_CONTROL2_1(DOMStorageHostMsg_StorageAreaId,
                            int64 /* namespace_id */,
                            string16 /* origin */,
                            int64 /* storage_area_id */)

// Get the length of a storage area.
IPC_SYNC_MESSAGE_CONTROL1_1(DOMStorageHostMsg_Length,
                            int64 /* storage_area_id */,
                            unsigned /* length */)

// Get a the ith key within a storage area.
IPC_SYNC_MESSAGE_CONTROL2_1(DOMStorageHostMsg_Key,
                            int64 /* storage_area_id */,
                            unsigned /* index */,
                            NullableString16 /* key */)

// Get a value based on a key from a storage area.
IPC_SYNC_MESSAGE_CONTROL2_1(DOMStorageHostMsg_GetItem,
                            int64 /* storage_area_id */,
                            string16 /* key */,
                            NullableString16 /* value */)

// Set a value that's associated with a key in a storage area.
IPC_SYNC_MESSAGE_CONTROL5_2(DOMStorageHostMsg_SetItem,
                            int /* routing_id */,
                            int64 /* storage_area_id */,
                            string16 /* key */,
                            string16 /* value */,
                            GURL /* url */,
                            WebKit::WebStorageArea::Result /* result */,
                            NullableString16 /* old_value */)

// Remove the value associated with a key in a storage area.
IPC_SYNC_MESSAGE_CONTROL3_1(DOMStorageHostMsg_RemoveItem,
                            int64 /* storage_area_id */,
                            string16 /* key */,
                            GURL /* url */,
                            NullableString16 /* old_value */)

// Clear the storage area.
IPC_SYNC_MESSAGE_CONTROL2_1(DOMStorageHostMsg_Clear,
                            int64 /* storage_area_id */,
                            GURL /* url */,
                            bool /* something_cleared */)

