// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include "ipc/ipc_message_macros.h"
#include "chrome/common/safe_browsing/ipc_protobuf_message_macros.h"
#include "chrome/common/safe_browsing/protobuf_message_param_traits.h"

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(SubMessage)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(foo)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(TestMessage)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(fund_int)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(op_comp_string)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(op_comp_bytes)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(op_comp_sub)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(rep_comp_sub)
IPC_PROTOBUF_MESSAGE_TRAITS_END()
