// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"
#include "webkit/glue/web_intent_service_data.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START IntentsMsgStart

IPC_ENUM_TRAITS(webkit_glue::WebIntentReplyType)
IPC_ENUM_TRAITS(webkit_glue::WebIntentData::DataType)
IPC_ENUM_TRAITS(webkit_glue::WebIntentServiceData::Disposition)

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::WebIntentData)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(extra_data)
  IPC_STRUCT_TRAITS_MEMBER(service)
  IPC_STRUCT_TRAITS_MEMBER(suggestions)
  IPC_STRUCT_TRAITS_MEMBER(unserialized_data)
  IPC_STRUCT_TRAITS_MEMBER(message_port_ids)
  IPC_STRUCT_TRAITS_MEMBER(blob_file)
  IPC_STRUCT_TRAITS_MEMBER(blob_length)
  IPC_STRUCT_TRAITS_MEMBER(root_path)
  IPC_STRUCT_TRAITS_MEMBER(filesystem_id)
  IPC_STRUCT_TRAITS_MEMBER(data_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::WebIntentServiceData)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(scheme)
  IPC_STRUCT_TRAITS_MEMBER(service_url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(disposition)
IPC_STRUCT_TRAITS_END()

// Set the intent data to be set on the service page.
IPC_MESSAGE_ROUTED1(IntentsMsg_SetWebIntentData,
                    webkit_glue::WebIntentData)

// Send the service's reply to the client page.
IPC_MESSAGE_ROUTED3(IntentsMsg_WebIntentReply,
                    webkit_glue::WebIntentReplyType /* reply type */,
                    string16 /* payload data */,
                    int /* intent ID */)

// Notify the container that the service has replied to the client page.
IPC_MESSAGE_ROUTED2(IntentsHostMsg_WebIntentReply,
                    webkit_glue::WebIntentReplyType /* reply type */,
                    string16 /* payload data */)

// Route the startActivity Intents call from a page to the service picker.
IPC_MESSAGE_ROUTED2(IntentsHostMsg_WebIntentDispatch,
                    webkit_glue::WebIntentData,
                    int /* intent ID */)

// Register a new service for Intents with the given action and type filter.
IPC_MESSAGE_ROUTED2(IntentsHostMsg_RegisterIntentService,
                    webkit_glue::WebIntentServiceData,
                    bool /* user_gesture */)
