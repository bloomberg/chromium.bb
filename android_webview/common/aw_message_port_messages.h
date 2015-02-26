// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <vector>

#include "base/basictypes.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START AwMessagePortMsgStart

//-----------------------------------------------------------------------------
// MessagePort messages
// These are messages sent from the browser to the renderer process.

// Normally the postmessages are exchanged between the renderers and the message
// itself is opaque to the browser process. The format of the message is a
// WebSerializesScriptValue.  A WebSerializedScriptValue is a blink structure
// and can only be serialized/deserialized in renderer. Further, we could not
// have Blink or V8 on the browser side due to their relience on static
// variables.
//
// For posting messages from Java (Webview) to JS, we pass the browser/renderer
// boundary an extra time and convert the messages to a type that browser can
// use. Since WebView is single-process this is not terribly expensive, but
// if we can do the conversion at the browser, then we can drop this code.

// Important Note about multi-process situation: Webview is single process so
// such a conversion does not increase the risk due to untrusted renderers.
// However, in a multi-process scenario, the renderer that does the conversion
// can be different then the renderer that receives the message. There are
// 2 possible solutions to deal with this:
// 1. Do the conversion at the browser side by writing a new serializer
// deserializer for WebSerializedScriptValue
// 2. Do the conversion at the content layer, at the renderer at the time of
// receiveing the message. This may need adding new flags to indicate that
// message needs to be converted. However, this is complicated due to queing
// at the browser side and possibility of ports being shipped to a different
// renderer or browser delegate.


// Tells the renderer to convert the message from a WebSerializeScript
// format to a base::ListValue. This IPC is used for messages that are
// incoming to Android webview from JS.
IPC_MESSAGE_ROUTED3(AwMessagePortMsg_WebToAppMessage,
                    int /* recipient message port id */,
                    base::string16 /* message */,
                    std::vector<int> /* sent message port_ids */)

// Tells the renderer to convert the message from a String16
// format to a WebSerializedScriptValue. This IPC is used for messages that
// are outgoing from Webview to JS.
// TODO(sgurun) when we start supporting other types, use a ListValue instead
// of string16
IPC_MESSAGE_ROUTED3(AwMessagePortMsg_AppToWebMessage,
                    int /* recipient message port id */,
                    base::string16 /* message */,
                    std::vector<int> /* sent message port_ids */)

// Used to defer message port closing until after all in-flight messages
// are flushed from renderer to browser. Renderer piggy-backs the message
// to browser.
IPC_MESSAGE_ROUTED1(AwMessagePortMsg_ClosePort,
                    int /* message port id */)

//-----------------------------------------------------------------------------
// These are messages sent from the renderer to the browser process.

// Response to AwMessagePortMessage_WebToAppMessage
IPC_MESSAGE_ROUTED3(AwMessagePortHostMsg_ConvertedWebToAppMessage,
                    int /* recipient message port id */,
                    base::ListValue /* converted message */,
                    std::vector<int> /* sent message port_ids */)

// Response to AwMessagePortMessage_AppToWebMessage
IPC_MESSAGE_ROUTED3(AwMessagePortHostMsg_ConvertedAppToWebMessage,
                    int /* recipient message port id */,
                    base::string16 /* converted message */,
                    std::vector<int> /* sent message port_ids */)

// Response to AwMessagePortMsg_ClosePort
IPC_MESSAGE_ROUTED1(AwMessagePortHostMsg_ClosePortAck,
                    int /* message port id */)
