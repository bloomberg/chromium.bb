// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "webkit/glue/web_intent_data.h"

#define IPC_MESSAGE_START IntentsMsgStart

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::WebIntentData)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

// Define enums used in this file inside an include-guard.
#ifndef CONTENT_COMMON_INTENTS_MESSAGES_H_
#define CONTENT_COMMON_INTENTS_MESSAGES_H_

// Constant values use to indicate what type of reply the caller is getting from
// the web intents service page.
struct IntentsMsg_WebIntentReply_Type {
 public:
  enum Value {
    // Sent for a reply message (success).
    Reply,

    // Sent for a failure message.
    Failure,
  };
};

#endif  // CONTENT_COMMON_INTENTS_MESSAGES_H_

IPC_ENUM_TRAITS(IntentsMsg_WebIntentReply_Type::Value)

IPC_MESSAGE_ROUTED2(IntentsMsg_SetWebIntentData,
                    webkit_glue::WebIntentData,
                    int /* intent ID */)

IPC_MESSAGE_ROUTED3(IntentsMsg_WebIntentReply,
                    IntentsMsg_WebIntentReply_Type::Value /* reply type */,
                    string16 /* payload data */,
                    int /* intent ID */)

// Route the startActivity Intents call from a page to the service picker.
IPC_MESSAGE_ROUTED2(IntentsHostMsg_WebIntentDispatch,
                    webkit_glue::WebIntentData,
                    int /* intent ID */)
