// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

#define IPC_MESSAGE_START IntentsMsgStart

IPC_ENUM_TRAITS(webkit_glue::WebIntentReplyType)

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::WebIntentData)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_MESSAGE_ROUTED2(IntentsMsg_SetWebIntentData,
                    webkit_glue::WebIntentData,
                    int /* intent ID */)

IPC_MESSAGE_ROUTED3(IntentsMsg_WebIntentReply,
                    webkit_glue::WebIntentReplyType /* reply type */,
                    string16 /* payload data */,
                    int /* intent ID */)

// Route the startActivity Intents call from a page to the service picker.
IPC_MESSAGE_ROUTED2(IntentsHostMsg_WebIntentDispatch,
                    webkit_glue::WebIntentData,
                    int /* intent ID */)

// Register a new service for Intents with the given action and type filter.
IPC_MESSAGE_ROUTED5(IntentsHostMsg_RegisterIntentService,
                    string16 /* action */,
                    string16 /* type */,
                    string16 /* href */,
                    string16 /* title */,
                    string16 /* disposition */)
