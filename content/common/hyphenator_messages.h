// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for hyphenation.
// Message definition file, included multiple times, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START HyphenatorMsgStart

// Opens the specified hyphenation dictionary. This message is expected to be
// sent when WebKit calls the canHyphenate function, i.e. when it starts
// layouting text. At this time, WebKit does not actually need this dictionary
// to hyphenate words. Therefore, a renderer does not need to wait for a browser
// to open the specified dictionary.
IPC_MESSAGE_CONTROL1(HyphenatorHostMsg_OpenDictionary,
                     string16 /* locale */)

// Sends the hyphenation dictionary to the renderer. This messages is sent in
// response to a HyphenatorHostMsg_OpenDictionary message.
IPC_MESSAGE_CONTROL1(HyphenatorMsg_SetDictionary,
                     IPC::PlatformFileForTransit /* dict_file */)
