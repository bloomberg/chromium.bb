// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_DOM_STORAGE_MESSAGES_H_
#define CHROME_COMMON_DOM_STORAGE_MESSAGES_H_
#pragma once

#include "chrome/common/dom_storage_common.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"

#define IPC_MESSAGE_START DOMStorageMsgStart

// Signals a storage event.
struct DOMStorageMsg_Event_Params {
  DOMStorageMsg_Event_Params();
  ~DOMStorageMsg_Event_Params();

  // The key that generated the storage event.  Null if clear() was called.
  NullableString16 key;

  // The old value of this key.  Null on clear() or if it didn't have a value.
  NullableString16 old_value;

  // The new value of this key.  Null on removeItem() or clear().
  NullableString16 new_value;

  // The origin this is associated with.
  string16 origin;

  // The URL of the page that caused the storage event.
  GURL url;

  // The storage type of this event.
  DOMStorageType storage_type;
};

namespace IPC {

template <>
struct ParamTraits<DOMStorageMsg_Event_Params> {
  typedef DOMStorageMsg_Event_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<DOMStorageType> {
  typedef DOMStorageType param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebStorageArea::Result> {
  typedef WebKit::WebStorageArea::Result param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

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

#endif  // CHROME_COMMON_DOM_STORAGE_MESSAGES_H_
