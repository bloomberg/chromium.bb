// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

// This file defines the IPCs for the browser-side implementation of
// WebSockets. For the legacy renderer-side implementation, see
// socket_stream_messages.h.
// TODO(ricea): Fix this comment when the legacy implementation has been
// removed.
//
// This IPC interface is based on the WebSocket multiplexing draft spec,
// http://tools.ietf.org/html/draft-ietf-hybi-websocket-multiplexing-09

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/websocket.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START WebSocketMsgStart

// WebSocket messages sent from the renderer to the browser.

// Open new virtual WebSocket connection to |socket_url|. |channel_id| is an
// identifier chosen by the renderer for the new channel. It cannot correspond
// to an existing open channel, and must be between 1 and
// 0x7FFFFFFF. |requested_protocols| is a list of tokens identifying
// sub-protocols the renderer would like to use, as described in RFC6455
// "Subprotocols Using the WebSocket Protocol".
//
// The browser process will not send |channel_id| as-is to the remote server; it
// will try to use a short id on the wire. This saves the renderer from
// having to try to choose the ids cleverly.
IPC_MESSAGE_CONTROL4(WebSocketHostMsg_AddChannelRequest,
                     int /* channel_id */,
                     GURL /* socket_url */,
                     std::vector<std::string> /* requested_protocols */,
                     GURL /* origin */)

// Web Socket messages sent from the browser to the renderer.

// Respond to an AddChannelRequest for channel |channel_id|. |channel_id| is
// scoped to the renderer process; while it is unique per-renderer, the browser
// may have multiple renderers using the same id. If |fail| is true, the channel
// could not be established (the cause of the failure is not provided to the
// renderer in order to limit its ability to abuse WebSockets to perform network
// probing, etc.). If |fail| is set then the |channel_id| is available for
// re-use. |selected_protocol| is the sub-protocol the server selected,
// or empty if no sub-protocol was selected. |extensions| is the list of
// extensions negotiated for the connection.
IPC_MESSAGE_CONTROL4(WebSocketMsg_AddChannelResponse,
                     int /* channel_id */,
                     bool /* fail */,
                     std::string /* selected_protocol */,
                     std::string /* extensions */)

// WebSocket messages that can be sent in either direction.

IPC_ENUM_TRAITS(content::WebSocketMessageType)

// Send a non-control frame on |channel_id|. If the sender is the renderer, it
// will be sent to the remote server. If the sender is the browser, it comes
// from the remote server. |fin| indicates that this frame is the last in the
// current message. |type| is the type of the message. On the first frame of a
// message, it must be set to either WEB_SOCKET_MESSAGE_TYPE_TEXT or
// WEB_SOCKET_MESSAGE_TYPE_BINARY. On subsequent frames, it must be set to
// WEB_SOCKET_MESSAGE_TYPE_CONTINUATION, and the type is the same as that of the
// first message. If |type| is WEB_SOCKET_MESSAGE_TYPE_TEXT, then the
// concatenation of the |data| from every frame in the message must be valid
// UTF-8. If |fin| is not set, |data| must be non-empty.
IPC_MESSAGE_CONTROL4(WebSocketMsg_SendFrame,
                     int /* channel_id */,
                     bool /* fin */,
                     content::WebSocketMessageType /* type */,
                     std::vector<char> /* data */)

// Add |quota| tokens of send quota for channel |channel_id|. |quota| must be a
// positive integer. Both the browser and the renderer set send quota for the
// other side, and check that quota has not been exceeded when receiving
// messages. Both sides start a new channel with a quota of 0, and must wait for
// a FlowControl message before calling SendFrame. The total available quota on
// one side must never exceed 0x7FFFFFFFFFFFFFFF tokens.
IPC_MESSAGE_CONTROL2(WebSocketMsg_FlowControl,
                     int /* channel_id */,
                     int64 /* quota */)

// Drop the channel.
// When sent by the renderer, this will cause a DropChannel message to be sent
// if the multiplex extension is in use, otherwise a Close message will be sent
// and the TCP/IP connection will be closed.
// When sent by the browser, this indicates that a Close or DropChannel has been
// received, the connection was closed, or a network or protocol error
// occurred. On receiving DropChannel, the renderer process may consider the
// |channel_id| available for reuse by a new AddChannelRequest.
// |code| is one of the reason codes specified in RFC6455 or
// draft-ietf-hybi-websocket-multiplexing-09. |reason|, if non-empty, is a
// UTF-8 encoded string which may be useful for debugging but is not necessarily
// human-readable, as supplied by the server in the Close or DropChannel
// message.
IPC_MESSAGE_CONTROL3(WebSocketMsg_DropChannel,
                     int /* channel_id */,
                     unsigned short /* code */,
                     std::string /* reason */)
