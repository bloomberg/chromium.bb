// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file ipc_messsage_macros.h defines the classes for individual
// messages. This file works similarly, except that it defines the
// implementations of the constructors and the logging methods. (These only
// have to be generated once). It is meant to be included in a XXX_messages.cc
// file.
//
// Unlike ipc_message_macros.h, this file is only included once; it uses one
// pass. But we *still* can't use normal include guards because we still need
// to use the MESSAGES_INTERNAL_FILE dispatch system. Because that #define is
// unset, we use the different name MESSAGES_INTERNAL_IMPL_FILE to tell this
// file what to do.
#ifndef IPC_IPC_MESSAGE_IMPL_MACROS_H_
#define IPC_IPC_MESSAGE_IMPL_MACROS_H_

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_message_utils_impl.h"

#ifndef MESSAGES_INTERNAL_IMPL_FILE
#error This file should only be included by X_messages.cc, which needs to define MESSAGES_INTERNAL_IMPL_FILE first.
#endif

// Trick scons and xcode into seeing the possible real dependencies since they
// don't understand #include MESSAGES_INTERNAL_FILE. See http://crbug.com/7828
#if 0
#include "ipc/ipc_sync_message_unittest.h"
#include "chrome/common/plugin_messages_internal.h"
#include "chrome/common/render_messages_internal.h"
#include "chrome/common/devtools_messages_internal.h"
#include "chrome/test/automation/automation_messages_internal.h"
#include "chrome/common/worker_messages_internal.h"
#endif

// These are probalby still defined because of ipc_message_macros.h should be
// included before us for the class/method declarations.
#undef IPC_BEGIN_MESSAGES
#undef IPC_END_MESSAGES
#undef IPC_MESSAGE_CONTROL0
#undef IPC_MESSAGE_CONTROL1
#undef IPC_MESSAGE_CONTROL2
#undef IPC_MESSAGE_CONTROL3
#undef IPC_MESSAGE_CONTROL4
#undef IPC_MESSAGE_CONTROL5
#undef IPC_MESSAGE_ROUTED0
#undef IPC_MESSAGE_ROUTED1
#undef IPC_MESSAGE_ROUTED2
#undef IPC_MESSAGE_ROUTED3
#undef IPC_MESSAGE_ROUTED4
#undef IPC_MESSAGE_ROUTED5
#undef IPC_SYNC_MESSAGE_CONTROL0_0
#undef IPC_SYNC_MESSAGE_CONTROL0_1
#undef IPC_SYNC_MESSAGE_CONTROL0_2
#undef IPC_SYNC_MESSAGE_CONTROL0_3
#undef IPC_SYNC_MESSAGE_CONTROL1_0
#undef IPC_SYNC_MESSAGE_CONTROL1_1
#undef IPC_SYNC_MESSAGE_CONTROL1_2
#undef IPC_SYNC_MESSAGE_CONTROL1_3
#undef IPC_SYNC_MESSAGE_CONTROL2_0
#undef IPC_SYNC_MESSAGE_CONTROL2_1
#undef IPC_SYNC_MESSAGE_CONTROL2_2
#undef IPC_SYNC_MESSAGE_CONTROL2_3
#undef IPC_SYNC_MESSAGE_CONTROL3_1
#undef IPC_SYNC_MESSAGE_CONTROL3_2
#undef IPC_SYNC_MESSAGE_CONTROL3_3
#undef IPC_SYNC_MESSAGE_CONTROL4_1
#undef IPC_SYNC_MESSAGE_CONTROL4_2
#undef IPC_SYNC_MESSAGE_ROUTED0_0
#undef IPC_SYNC_MESSAGE_ROUTED0_1
#undef IPC_SYNC_MESSAGE_ROUTED0_2
#undef IPC_SYNC_MESSAGE_ROUTED0_3
#undef IPC_SYNC_MESSAGE_ROUTED1_0
#undef IPC_SYNC_MESSAGE_ROUTED1_1
#undef IPC_SYNC_MESSAGE_ROUTED1_2
#undef IPC_SYNC_MESSAGE_ROUTED1_3
#undef IPC_SYNC_MESSAGE_ROUTED1_4
#undef IPC_SYNC_MESSAGE_ROUTED2_0
#undef IPC_SYNC_MESSAGE_ROUTED2_1
#undef IPC_SYNC_MESSAGE_ROUTED2_2
#undef IPC_SYNC_MESSAGE_ROUTED2_3
#undef IPC_SYNC_MESSAGE_ROUTED3_0
#undef IPC_SYNC_MESSAGE_ROUTED3_1
#undef IPC_SYNC_MESSAGE_ROUTED3_2
#undef IPC_SYNC_MESSAGE_ROUTED3_3
#undef IPC_SYNC_MESSAGE_ROUTED4_0
#undef IPC_SYNC_MESSAGE_ROUTED4_1
#undef IPC_SYNC_MESSAGE_ROUTED4_2
#undef IPC_SYNC_MESSAGE_ROUTED4_3
#undef IPC_SYNC_MESSAGE_ROUTED5_0
#undef IPC_SYNC_MESSAGE_ROUTED5_1
#undef IPC_SYNC_MESSAGE_ROUTED5_2
#undef IPC_SYNC_MESSAGE_ROUTED5_3

// These don't do anything during this pass.
#define IPC_BEGIN_MESSAGES(label)
#define IPC_END_MESSAGES(label)

// This derives from IPC::Message and thus doesn't need us to keep the
// implementations in this impl file.
#define IPC_MESSAGE_CONTROL0(msg_class)

#define IPC_MESSAGE_CONTROL1(msg_class, type1)                          \
  msg_class::msg_class(const type1& arg1)                               \
      : IPC::MessageWithTuple< Tuple1<type1> >(                         \
          MSG_ROUTING_CONTROL, ID, MakeRefTuple(arg1)) {}               \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

#define IPC_MESSAGE_CONTROL2(msg_class, type1, type2)                   \
  msg_class::msg_class(const type1& arg1, const type2& arg2)            \
      : IPC::MessageWithTuple< Tuple2<type1, type2> >(                  \
          MSG_ROUTING_CONTROL, ID, MakeRefTuple(arg1, arg2)) {}         \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

#define IPC_MESSAGE_CONTROL3(msg_class, type1, type2, type3)            \
  msg_class::msg_class(const type1& arg1, const type2& arg2,            \
                       const type3& arg3)                               \
      : IPC::MessageWithTuple< Tuple3<type1, type2, type3> >(           \
          MSG_ROUTING_CONTROL, ID, MakeRefTuple(arg1, arg2, arg3)) {}   \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

#define IPC_MESSAGE_CONTROL4(msg_class, type1, type2, type3, type4)    \
  msg_class::msg_class(const type1& arg1, const type2& arg2,           \
                       const type3& arg3, const type4& arg4)           \
      : IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> >(   \
          MSG_ROUTING_CONTROL, ID, MakeRefTuple(arg1, arg2, arg3, arg4)) {} \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

#define IPC_MESSAGE_CONTROL5(msg_class, type1, type2, type3, type4, type5) \
  msg_class::msg_class(const type1& arg1, const type2& arg2,            \
                       const type3& arg3, const type4& arg4,            \
                       const type5& arg5)                               \
      : IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, type5> >( \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeRefTuple(arg1, arg2, arg3, arg4, arg5)) {}                \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

// This derives from IPC::Message and thus doesn't need us to keep the
// implementations in this impl file.
#define IPC_MESSAGE_ROUTED0(msg_class)

#define IPC_MESSAGE_ROUTED1(msg_class, type1)                           \
  msg_class::msg_class(int32 routing_id, const type1& arg1)             \
      : IPC::MessageWithTuple< Tuple1<type1> >(                         \
          routing_id, ID, MakeRefTuple(arg1)) {}                        \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

#define IPC_MESSAGE_ROUTED2(msg_class, type1, type2)                    \
  msg_class::msg_class(int32 routing_id, const type1& arg1, const type2& arg2) \
      : IPC::MessageWithTuple< Tuple2<type1, type2> >(                  \
          routing_id, ID, MakeRefTuple(arg1, arg2)) {}                  \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }


#define IPC_MESSAGE_ROUTED3(msg_class, type1, type2, type3)             \
  msg_class::msg_class(int32 routing_id, const type1& arg1,             \
                       const type2& arg2, const type3& arg3)            \
      : IPC::MessageWithTuple< Tuple3<type1, type2, type3> >(           \
          routing_id, ID, MakeRefTuple(arg1, arg2, arg3)) {}            \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

#define IPC_MESSAGE_ROUTED4(msg_class, type1, type2, type3, type4)      \
  msg_class::msg_class(int32 routing_id, const type1& arg1, const type2& arg2, \
                       const type3& arg3, const type4& arg4)            \
      : IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> >(    \
          routing_id, ID, MakeRefTuple(arg1, arg2, arg3, arg4)) {}      \
                                                                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

#define IPC_MESSAGE_ROUTED5(msg_class, type1, type2, type3, type4, type5) \
  msg_class::msg_class(int32 routing_id, const type1& arg1,             \
                       const type2& arg2, const type3& arg3,            \
                       const type4& arg4, const type5& arg5)            \
      : IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, type5> >( \
          routing_id, ID, MakeRefTuple(arg1, arg2, arg3, arg4, arg5)) {} \
                                                                        \
  msg_class::~msg_class() { }                                           \
                                                                        \
  void msg_class::Log(const Message* msg, std::wstring* l) {            \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

// TODO(erg): Fill these in as I go along.
#define IPC_SYNC_MESSAGE_CONTROL0_0(msg_class)
#define IPC_SYNC_MESSAGE_CONTROL0_1(msg_class, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL0_2(msg_class, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL0_3(msg_class, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL1_0(msg_class, type1_in)
#define IPC_SYNC_MESSAGE_CONTROL1_1(msg_class, type1_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL1_2(msg_class, type1_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL1_3(msg_class, type1_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL2_0(msg_class, type1_in, type2_in)
#define IPC_SYNC_MESSAGE_CONTROL2_1(msg_class, type1_in, type2_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL2_2(msg_class, type1_in, type2_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL3_1(msg_class, type1_in, type2_in, type3_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED0_0(msg_class)
#define IPC_SYNC_MESSAGE_ROUTED0_1(msg_class, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED0_2(msg_class, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED0_3(msg_class, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED1_0(msg_class, type1_in)
#define IPC_SYNC_MESSAGE_ROUTED1_1(msg_class, type1_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED1_2(msg_class, type1_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED1_3(msg_class, type1_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED2_0(msg_class, type1_in, type2_in)
#define IPC_SYNC_MESSAGE_ROUTED2_1(msg_class, type1_in, type2_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED2_2(msg_class, type1_in, type2_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED3_0(msg_class, type1_in, type2_in, type3_in)
#define IPC_SYNC_MESSAGE_ROUTED3_1(msg_class, type1_in, type2_in, type3_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED4_0(msg_class, type1_in, type2_in, type3_in, type4_in)
#define IPC_SYNC_MESSAGE_ROUTED4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED4_3(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED5_0(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in)
#define IPC_SYNC_MESSAGE_ROUTED5_1(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED5_2(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED5_3(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out)

// Trigger the header guard define in ipc_message_macros.h so we don't get
// duplicate including when we include MESSAGES_INTERNAL_FILE again at the end
// of this file.
#define IPC_MESSAGE_MACROS_INCLUDE_BLOCK

// Redefine MESSAGES_INTERNAL_FILE just for the header check in
// ipc_messages_macros.h that happens before it breaks on the header guard.
#define MESSAGES_INTERNAL_FILE MESSAGES_INTERNAL_IMPL_FILE

// Include our INTERNAL file first to get the normal expansion.
#include MESSAGES_INTERNAL_IMPL_FILE

#endif  // IPC_IPC_MESSAGE_IMPL_MACROS_H_
