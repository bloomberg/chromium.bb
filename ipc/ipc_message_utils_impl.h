// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains templates forward declared (but not defined) in
// ipc_message_utils.h so that they are only instantiated in certain files,
// notably a few IPC unit tests.

#ifndef IPC_IPC_MESSAGE_UTILS_IMPL_H_
#define IPC_IPC_MESSAGE_UTILS_IMPL_H_

namespace IPC {

template <class ParamType>
MessageWithTuple<ParamType>::MessageWithTuple(
    int32 routing_id, uint32 type, const RefParam& p)
    : Message(routing_id, type, PRIORITY_NORMAL) {
  WriteParam(this, p);
}

template <class ParamType>
bool MessageWithTuple<ParamType>::Read(const Message* msg, Param* p) {
  void* iter = NULL;
  if (ReadParam(msg, &iter, p))
    return true;
  NOTREACHED() << "Error deserializing message " << msg->type();
  return false;
}

// We can't migrate the template for Log() to MessageWithTuple, because each
// subclass needs to have Log() to call Read(), which instantiates the above
// template.

template <class SendParamType, class ReplyParamType>
MessageWithReply<SendParamType, ReplyParamType>::MessageWithReply(
    int32 routing_id, uint32 type,
    const RefSendParam& send,
    const ReplyParam& reply)
    : SyncMessage(routing_id, type, PRIORITY_NORMAL,
                  new ParamDeserializer<ReplyParam>(reply)) {
  WriteParam(this, send);
}

template <class SendParamType, class ReplyParamType>
bool MessageWithReply<SendParamType, ReplyParamType>::ReadSendParam(
    const Message* msg, SendParam* p) {
  void* iter = SyncMessage::GetDataIterator(msg);
  return ReadParam(msg, &iter, p);
}

template <class SendParamType, class ReplyParamType>
bool MessageWithReply<SendParamType, ReplyParamType>::ReadReplyParam(
    const Message* msg, typename TupleTypes<ReplyParam>::ValueTuple* p) {
  void* iter = SyncMessage::GetDataIterator(msg);
  return ReadParam(msg, &iter, p);
}

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_UTILS_IMPL_H_
