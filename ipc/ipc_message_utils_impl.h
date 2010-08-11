// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains templates forward declared (but not defined) in
// ipc_message_utils.h so that they are only instantiated in certain files,
// notably ipc_message_impl_macros.h and a few IPC unit tests.

#ifndef IPC_IPC_MESSAGE_UTILS_IMPL_H_
#define IPC_IPC_MESSAGE_UTILS_IMPL_H_

namespace IPC {

template <class ParamType>
MessageWithTuple<ParamType>::MessageWithTuple(
    int32 routing_id, uint32 type, const RefParam& p)
    : Message(routing_id, type, PRIORITY_NORMAL) {
  WriteParam(this, p);
}

// TODO(erg): Migrate MessageWithTuple<ParamType>::Read() here once I figure
// out why having the definition here doesn't export the symbols.

// We can't migrate the template for Log() to MessageWithTuple, because each
// subclass needs to have Log() to call Read(), which instantiates the above
// template.

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_UTILS_IMPL_H_
