// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for navigator.connect

#include "content/common/navigator_connect_types.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START NavigatorConnectMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::CrossOriginServiceWorkerClient)
  IPC_STRUCT_TRAITS_MEMBER(target_url)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(message_port_id)
IPC_STRUCT_TRAITS_END()

// Messages sent from the child process to the browser.
IPC_MESSAGE_CONTROL3(NavigatorConnectHostMsg_Connect,
                     int /* thread_id */,
                     int /* request_id */,
                     content::CrossOriginServiceWorkerClient /* client */)

// Messages sent from the browser to the child process.
IPC_MESSAGE_CONTROL3(NavigatorConnectMsg_ConnectResult,
                     int /* thread_id */,
                     int /* request_id */,
                     bool /* allow_connect */)
