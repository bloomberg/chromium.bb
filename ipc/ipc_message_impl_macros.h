// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file ipc_messsage_macros.h defines the classes for individual
// messages. This file works similarly, except that it defines the
// implementations of the constructors and the logging methods. (These only
// have to be generated once). It is meant to be included in a XXX_messages.cc
// file right before including XXX_messages_internal.h.

#ifndef IPC_IPC_MESSAGE_IMPL_MACROS_H_
#define IPC_IPC_MESSAGE_IMPL_MACROS_H_

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_message_utils_impl.h"

// Every class must include a destructor and a log method that is keyed to the
// specific types.
#define IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)                       \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(std::string* name, const Message* msg, std::string* l) { \
    if (name)                                                           \
      *name = #msg_class;                                               \
    if (!msg || !l)                                                    \
      return;                                                           \
    Param p;                                                            \
    if (Read(msg, &p))                                                  \
      IPC::LogParam(p, l);                                              \
  }

// This derives from IPC::Message and thus doesn't need us to keep the
// implementations in this impl file.
#define IPC_MESSAGE_CONTROL0_EXTRA(msg_class)

#define IPC_MESSAGE_CONTROL1_EXTRA(msg_class, type1)                    \
  msg_class::msg_class(const type1& arg1)                               \
      : IPC::MessageWithTuple< Tuple1<type1> >(                         \
          MSG_ROUTING_CONTROL, ID, MakeRefTuple(arg1)) {}               \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_MESSAGE_CONTROL2_EXTRA(msg_class, type1, type2)             \
  msg_class::msg_class(const type1& arg1, const type2& arg2)            \
      : IPC::MessageWithTuple< Tuple2<type1, type2> >(                  \
          MSG_ROUTING_CONTROL, ID, MakeRefTuple(arg1, arg2)) {}         \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_MESSAGE_CONTROL3_EXTRA(msg_class, type1, type2, type3)      \
  msg_class::msg_class(const type1& arg1, const type2& arg2,            \
                       const type3& arg3)                               \
      : IPC::MessageWithTuple< Tuple3<type1, type2, type3> >(           \
          MSG_ROUTING_CONTROL, ID, MakeRefTuple(arg1, arg2, arg3)) {}   \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_MESSAGE_CONTROL4_EXTRA(msg_class, type1, type2, type3, type4) \
  msg_class::msg_class(const type1& arg1, const type2& arg2,           \
                       const type3& arg3, const type4& arg4)           \
      : IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> >(   \
          MSG_ROUTING_CONTROL, ID, MakeRefTuple(arg1, arg2, arg3, arg4)) {} \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_MESSAGE_CONTROL5_EXTRA(msg_class, type1, type2, type3, type4, type5) \
  msg_class::msg_class(const type1& arg1, const type2& arg2,            \
                       const type3& arg3, const type4& arg4,            \
                       const type5& arg5)                               \
      : IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, type5> >( \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeRefTuple(arg1, arg2, arg3, arg4, arg5)) {}                \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

// This derives from IPC::Message and thus doesn't need us to keep the
// implementations in this impl file.
#define IPC_MESSAGE_ROUTED0_EXTRA(msg_class)

#define IPC_MESSAGE_ROUTED1_EXTRA(msg_class, type1)                     \
  msg_class::msg_class(int32 routing_id, const type1& arg1)             \
      : IPC::MessageWithTuple< Tuple1<type1> >(                         \
          routing_id, ID, MakeRefTuple(arg1)) {}                        \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_MESSAGE_ROUTED2_EXTRA(msg_class, type1, type2)              \
  msg_class::msg_class(int32 routing_id, const type1& arg1, const type2& arg2) \
      : IPC::MessageWithTuple< Tuple2<type1, type2> >(                  \
          routing_id, ID, MakeRefTuple(arg1, arg2)) {}                  \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_MESSAGE_ROUTED3_EXTRA(msg_class, type1, type2, type3)       \
  msg_class::msg_class(int32 routing_id, const type1& arg1,             \
                       const type2& arg2, const type3& arg3)            \
      : IPC::MessageWithTuple< Tuple3<type1, type2, type3> >(           \
          routing_id, ID, MakeRefTuple(arg1, arg2, arg3)) {}            \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_MESSAGE_ROUTED4_EXTRA(msg_class, type1, type2, type3, type4) \
  msg_class::msg_class(int32 routing_id, const type1& arg1, const type2& arg2, \
                       const type3& arg3, const type4& arg4)            \
      : IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> >(    \
          routing_id, ID, MakeRefTuple(arg1, arg2, arg3, arg4)) {}      \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_MESSAGE_ROUTED5_EXTRA(msg_class, type1, type2, type3, type4, type5) \
  msg_class::msg_class(int32 routing_id, const type1& arg1,             \
                       const type2& arg2, const type3& arg3,            \
                       const type4& arg4, const type5& arg5)            \
      : IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, type5> >( \
          routing_id, ID, MakeRefTuple(arg1, arg2, arg3, arg4, arg5)) {} \
                                                                        \
  IPC_ASYNC_MESSAGE_DTOR_AND_LOG(msg_class)

// -----------------------------------------------------------------------------

// Every class must include a destructor and a log method that is keyed to the
// specific types.
#define IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)                        \
  msg_class::~msg_class() {}                                            \
                                                                        \
  void msg_class::Log(std::string* name, const Message* msg, std::string* l) { \
    if (name)                                                           \
      *name = #msg_class;                                               \
    if (!msg || !l)                                                     \
      return;                                                           \
    if (msg->is_sync()) {                                               \
      TupleTypes<SendParam>::ValueTuple p;                              \
      if (ReadSendParam(msg, &p))                                       \
        IPC::LogParam(p, l);                                            \
      AddOutputParamsToLog(msg, l);                                     \
    } else {                                                            \
      TupleTypes<ReplyParam>::ValueTuple p;                             \
      if (ReadReplyParam(msg, &p))                                      \
        IPC::LogParam(p, l);                                            \
    }                                                                   \
  }

#define IPC_SYNC_MESSAGE_CONTROL0_0_EXTRA(msg_class)                    \
  msg_class::msg_class()                                                \
      : IPC::MessageWithReply<Tuple0, Tuple0 >(                         \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeTuple(), MakeTuple()) {}                                  \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_1_EXTRA(msg_class, type1_out)         \
  msg_class::msg_class(type1_out* arg1)                                 \
      : IPC::MessageWithReply<Tuple0, Tuple1<type1_out&> >(             \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeTuple(), MakeRefTuple(*arg1)) {}                          \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_2_EXTRA(msg_class, type1_out, type2_out) \
  msg_class::msg_class(type1_out* arg1, type2_out* arg2)                \
      : IPC::MessageWithReply<Tuple0, Tuple2<type1_out&, type2_out&> >( \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeTuple(), MakeRefTuple(*arg1, *arg2)) {}                   \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_3_EXTRA(msg_class, type1_out, type2_out, \
                                    type3_out)                          \
  msg_class::msg_class(type1_out* arg1, type2_out* arg2, type3_out* arg3) \
      : IPC::MessageWithReply<Tuple0, Tuple3<type1_out&, type2_out&,    \
                                             type3_out&> >(             \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeTuple(), MakeRefTuple(*arg1, *arg2, *arg3)) {}            \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_SYNC_MESSAGE_CONTROL1_0_EXTRA(msg_class, type1_in)          \
  msg_class::msg_class(const type1_in& arg1)                            \
      : IPC::MessageWithReply<Tuple1<type1_in>, Tuple0 >(               \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeRefTuple(arg1), MakeTuple()) {}                           \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_1_EXTRA(msg_class, type1_in, type1_out) \
  msg_class::msg_class(const type1_in& arg1, type1_out* arg2)           \
      : IPC::MessageWithReply<Tuple1<type1_in>, Tuple1<type1_out&> >(   \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeRefTuple(arg1), MakeRefTuple(*arg2)) {}                   \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_2_EXTRA(msg_class, type1_in, type1_out, type2_out) \
  msg_class::msg_class(const type1_in& arg1, type1_out* arg2, type2_out* arg3) \
      : IPC::MessageWithReply<Tuple1<type1_in>,                         \
                              Tuple2<type1_out&, type2_out&> >(         \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeRefTuple(arg1), MakeRefTuple(*arg2, *arg3)) {}            \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_3_EXTRA(msg_class, type1_in, type1_out, \
                                    type2_out, type3_out)               \
  msg_class::msg_class(const type1_in& arg1, type1_out* arg2,           \
                       type2_out* arg3, type3_out* arg4)                \
      : IPC::MessageWithReply<Tuple1<type1_in>,                         \
          Tuple3<type1_out&, type2_out&, type3_out&> >(                 \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeRefTuple(arg1), MakeRefTuple(*arg2, *arg3, *arg4)) {}     \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_0_EXTRA(msg_class, type1_in, type2_in) \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2)      \
      : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple0 >(     \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeRefTuple(arg1, arg2), MakeTuple()) {}                     \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_1_EXTRA(msg_class, type1_in, type2_in, type1_out) \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       type1_out* arg3)                                 \
      : IPC::MessageWithReply<Tuple2<type1_in, type2_in>,               \
                              Tuple1<type1_out&> >(                     \
          MSG_ROUTING_CONTROL, ID,                                      \
          MakeRefTuple(arg1, arg2), MakeRefTuple(*arg3)) {}             \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_SYNC_MESSAGE_CONTROL2_2_EXTRA(msg_class, type1_in, type2_in, \
                                    type1_out, type2_out)               \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       type1_out* arg3, type2_out* arg4)                \
      : IPC::MessageWithReply<Tuple2<type1_in, type2_in>,               \
            Tuple2<type1_out&, type2_out&> >(MSG_ROUTING_CONTROL, ID,   \
            MakeRefTuple(arg1, arg2), MakeRefTuple(*arg3, *arg4)) {}    \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_SYNC_MESSAGE_CONTROL2_3_EXTRA(msg_class, type1_in, type2_in, \
                                    type1_out, type2_out, type3_out)    \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       type1_out* arg3, type2_out* arg4, type3_out* arg5) \
      : IPC::MessageWithReply<Tuple2<type1_in, type2_in>,               \
            Tuple3<type1_out&, type2_out&, type3_out&> >(MSG_ROUTING_CONTROL, \
            ID,                                                         \
            MakeRefTuple(arg1, arg2), MakeRefTuple(*arg3, *arg4, *arg5)) {} \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_SYNC_MESSAGE_CONTROL3_1_EXTRA(msg_class, type1_in, type2_in, \
                                    type3_in, type1_out)                \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       const type3_in& arg3, type1_out* arg4)           \
      : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>,     \
            Tuple1<type1_out&> >(MSG_ROUTING_CONTROL, ID,               \
            MakeRefTuple(arg1, arg2, arg3), MakeRefTuple(*arg4)) {}     \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_2_EXTRA(msg_class, type1_in, type2_in, \
                                    type3_in, type1_out, type2_out)     \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       const type3_in& arg3, type1_out* arg4, type2_out* arg5) \
      : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>,     \
          Tuple2<type1_out&, type2_out&> >(MSG_ROUTING_CONTROL, ID,     \
          MakeRefTuple(arg1, arg2, arg3), MakeRefTuple(*arg4, *arg5)) {} \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_SYNC_MESSAGE_CONTROL3_3_EXTRA(msg_class, type1_in, type2_in, \
                                    type3_in, type1_out, type2_out,     \
                                    type3_out)                          \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       const type3_in& arg3, type1_out* arg4,           \
                       type2_out* arg5, type3_out* arg6)                \
      : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>,     \
            Tuple3<type1_out&, type2_out&, type3_out&> >(MSG_ROUTING_CONTROL, \
            ID,                                                         \
            MakeRefTuple(arg1, arg2, arg3),                             \
            MakeRefTuple(*arg4, *arg5, *arg6)) {}                       \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_4_EXTRA(msg_class, type1_in, type2_in, \
                                    type3_in, type1_out, type2_out,     \
                                    type3_out, type4_out)               \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       const type3_in& arg3, type1_out* arg4,           \
                       type2_out* arg5, type3_out* arg6, type4_out* arg7) \
      : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>,     \
            Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >(   \
                MSG_ROUTING_CONTROL,                                    \
                ID,                                                     \
                MakeRefTuple(arg1, arg2, arg3),                         \
                MakeRefTuple(*arg4, *arg5, *arg6, *arg7)) {}            \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL4_1_EXTRA(msg_class, type1_in, type2_in, \
                                    type3_in, type4_in, type1_out)      \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       const type3_in& arg3, const type4_in& arg4,      \
                       type1_out* arg6)                                 \
      : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
            Tuple1<type1_out&> >(MSG_ROUTING_CONTROL, ID,               \
            MakeRefTuple(arg1, arg2, arg3, arg4), MakeRefTuple(*arg6)) {} \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_SYNC_MESSAGE_CONTROL4_2_EXTRA(msg_class, type1_in, type2_in, \
                                    type3_in, type4_in, type1_out,      \
                                    type2_out)                          \
  msg_class::msg_class(const type1_in& arg1, const type2_in& arg2,      \
                       const type3_in& arg3, const type4_in& arg4,      \
                       type1_out* arg5, type2_out* arg6)                \
      : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in,      \
                                     type4_in>,                         \
          Tuple2<type1_out&, type2_out&> >(MSG_ROUTING_CONTROL, ID,     \
          MakeRefTuple(arg1, arg2, arg3, arg4),                         \
          MakeRefTuple(*arg5, *arg6)) {}                                \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_0_EXTRA(msg_class)                     \
  msg_class::msg_class(int routing_id)                                  \
      : IPC::MessageWithReply<Tuple0, Tuple0>(                          \
            routing_id, ID,                                             \
            MakeTuple(), MakeTuple()) {}                                \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_1_EXTRA(msg_class, type1_out)          \
  msg_class::msg_class(int routing_id, type1_out* arg1)                 \
      : IPC::MessageWithReply<Tuple0, Tuple1<type1_out&> >(             \
          routing_id, ID,                                               \
          MakeTuple(), MakeRefTuple(*arg1)) {}                          \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_2_EXTRA(msg_class, type1_out, type2_out) \
  msg_class::msg_class(int routing_id, type1_out* arg1, type2_out* arg2) \
        : IPC::MessageWithReply<Tuple0, Tuple2<type1_out&, type2_out&> >( \
            routing_id, ID,                                             \
            MakeTuple(), MakeRefTuple(*arg1, *arg2)) {}                 \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_3_EXTRA(msg_class, type1_out, type2_out, \
                                   type3_out)                           \
  msg_class::msg_class(int routing_id, type1_out* arg1, type2_out* arg2, \
                       type3_out* arg3)                                 \
      : IPC::MessageWithReply<Tuple0,                                   \
          Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID,  \
          MakeTuple(), MakeRefTuple(*arg1, *arg2, *arg3)) {}            \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_0_EXTRA(msg_class, type1_in)           \
  msg_class::msg_class(int routing_id, const type1_in& arg1)            \
      : IPC::MessageWithReply<Tuple1<type1_in>, Tuple0 >(               \
          routing_id, ID,                                               \
          MakeRefTuple(arg1), MakeTuple()) {}                           \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_1_EXTRA(msg_class, type1_in, type1_out) \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       type1_out* arg2)                                 \
      : IPC::MessageWithReply<Tuple1<type1_in>, Tuple1<type1_out&> >(   \
          routing_id, ID,                                               \
          MakeRefTuple(arg1), MakeRefTuple(*arg2)) {}                   \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_2_EXTRA(msg_class, type1_in, type1_out, \
                                   type2_out)                           \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       type1_out* arg2, type2_out* arg3)                \
      : IPC::MessageWithReply<Tuple1<type1_in>,                         \
                              Tuple2<type1_out&, type2_out&> >(         \
          routing_id, ID,                                               \
          MakeRefTuple(arg1), MakeRefTuple(*arg2, *arg3)) {}            \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_3_EXTRA(msg_class, type1_in, type1_out, \
                                   type2_out, type3_out)                \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       type1_out* arg2, type2_out* arg3, type3_out* arg4) \
      : IPC::MessageWithReply<Tuple1<type1_in>,                         \
          Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID,  \
          MakeRefTuple(arg1), MakeRefTuple(*arg2, *arg3, *arg4)) {}     \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_4_EXTRA(msg_class, type1_in, type1_out, \
                                   type2_out, type3_out, type4_out)     \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       type1_out* arg2, type2_out* arg3,                \
                       type3_out* arg4, type4_out* arg5)                \
      : IPC::MessageWithReply<Tuple1<type1_in>,                         \
            Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >(   \
            routing_id, ID, MakeRefTuple(arg1),                         \
            MakeRefTuple(*arg2, *arg3, *arg4, *arg5)) {}                \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_0_EXTRA(msg_class, type1_in, type2_in) \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2)                            \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple0 >(   \
            routing_id, ID, MakeRefTuple(arg1, arg2), MakeTuple()) {}   \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_1_EXTRA(msg_class, type1_in, type2_in, \
                                   type1_out)                           \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, type1_out* arg3)           \
      : IPC::MessageWithReply<Tuple2<type1_in, type2_in>,               \
                              Tuple1<type1_out&> >(routing_id, ID,      \
          MakeRefTuple(arg1, arg2), MakeRefTuple(*arg3)) {}             \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_2_EXTRA(msg_class, type1_in, type2_in, \
                                   type1_out, type2_out)                \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, type1_out* arg3,           \
                       type2_out* arg4)                                 \
      : IPC::MessageWithReply<Tuple2<type1_in, type2_in>,               \
            Tuple2<type1_out&, type2_out&> >(routing_id, ID,            \
            MakeRefTuple(arg1, arg2), MakeRefTuple(*arg3, *arg4)) {}    \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_3_EXTRA(msg_class, type1_in, type2_in, \
                                   type1_out, type2_out, type3_out)     \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, type1_out* arg3,           \
                       type2_out* arg4, type3_out* arg5)                \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>,             \
            Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID, \
            MakeRefTuple(arg1, arg2), MakeRefTuple(*arg3, *arg4, *arg5)) {} \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_0_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in)                            \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3)      \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, Tuple0>( \
            routing_id, ID,                                             \
            MakeRefTuple(arg1, arg2, arg3), MakeTuple()) {}             \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_1_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type1_out)                 \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       type1_out* arg4)                                 \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>,   \
            Tuple1<type1_out&> >(routing_id, ID,                        \
            MakeRefTuple(arg1, arg2, arg3), MakeRefTuple(*arg4)) {}     \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_2_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type1_out, type2_out)      \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       type1_out* arg4, type2_out* arg5)                \
      : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>,     \
          Tuple2<type1_out&, type2_out&> >(routing_id, ID,              \
          MakeRefTuple(arg1, arg2, arg3), MakeRefTuple(*arg4, *arg5)) {} \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_3_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type1_out, type2_out,      \
                                   type3_out)                           \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       type1_out* arg4, type2_out* arg5, type3_out* arg6) \
      : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>,     \
          Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID,  \
          MakeRefTuple(arg1, arg2, arg3), MakeRefTuple(*arg4, *arg5,    \
                                                       *arg6)) {}       \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_4_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type1_out, type2_out,      \
                                   type3_out, type4_out)                \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       type1_out* arg4, type2_out* arg5,                \
                       type3_out* arg6, type4_out* arg7) \
      : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>,     \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >(     \
          routing_id, ID, MakeRefTuple(arg1, arg2, arg3),               \
          MakeRefTuple(*arg4, *arg5, *arg6, *arg7)) {}                  \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_0_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type4_in)                  \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       const type4_in& arg4)                            \
        : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in,    \
            type4_in>, Tuple0 >(routing_id, ID,                         \
            MakeRefTuple(arg1, arg2, arg3, arg4), MakeTuple()) {}       \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_1_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type4_in, type1_out)       \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       const type4_in& arg4, type1_out* arg6)           \
        : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
            Tuple1<type1_out&> >(routing_id, ID,                        \
            MakeRefTuple(arg1, arg2, arg3, arg4), MakeRefTuple(*arg6)) {} \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_SYNC_MESSAGE_ROUTED4_2_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type4_in, type1_out,       \
                                   type2_out)                           \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       const type4_in& arg4, type1_out* arg5, type2_out* arg6) \
      : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple2<type1_out&, type2_out&> >(routing_id, ID,              \
          MakeRefTuple(arg1, arg2, arg3, arg4), MakeRefTuple(*arg5, *arg6)) {} \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)


#define IPC_SYNC_MESSAGE_ROUTED4_3_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type4_in, type1_out,       \
                                   type2_out, type3_out)                \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       const type4_in& arg4, type1_out* arg5,           \
                       type2_out* arg6, type3_out* arg7)                \
      : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID,  \
          MakeRefTuple(arg1, arg2, arg3, arg4),                         \
          MakeRefTuple(*arg5, *arg6, *arg7)) {}                         \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_0_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type4_in, type5_in)        \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       const type4_in& arg4, const type5_in& arg5)      \
      : IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in,      \
          type4_in, type5_in>, Tuple0 >(routing_id, ID,                 \
            MakeRefTuple(arg1, arg2, arg3, arg4, arg5), MakeTuple()) {} \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_1_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type4_in, type5_in, type1_out) \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       const type4_in& arg4, const type5_in& arg5,      \
                       type1_out* arg6)                                 \
      : IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in,      \
          type4_in, type5_in>, Tuple1<type1_out&> >(routing_id, ID,     \
          MakeRefTuple(arg1, arg2, arg3, arg4, arg5),                   \
          MakeRefTuple(*arg6)) {}                                       \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_2_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type4_in, type5_in,        \
                                   type1_out, type2_out)                \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       const type4_in& arg4, const type4_in& arg5,      \
                       type1_out* arg6, type2_out* arg7)                \
      : IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in,      \
          type4_in, type5_in>, Tuple2<type1_out&, type2_out&> >(        \
          routing_id, ID, MakeRefTuple(arg1, arg2, arg3, arg4, arg5),   \
          MakeRefTuple(*arg6, *arg7)) {}                                \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_3_EXTRA(msg_class, type1_in, type2_in, \
                                   type3_in, type4_in, type5_in,        \
                                   type1_out, type2_out, type3_out)     \
  msg_class::msg_class(int routing_id, const type1_in& arg1,            \
                       const type2_in& arg2, const type3_in& arg3,      \
                       const type4_in& arg4, const type4_in& arg5,      \
                       type1_out* arg6, type2_out* arg7,                \
                       type3_out* arg8)                                 \
      : IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in,      \
                                     type4_in, type5_in>,               \
          Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID,  \
          MakeRefTuple(arg1, arg2, arg3, arg4, arg5),                   \
          MakeRefTuple(*arg6, *arg7, *arg8)) {}                         \
                                                                        \
  IPC_SYNC_MESSAGE_DTOR_AND_LOG(msg_class)

#endif  // IPC_IPC_MESSAGE_IMPL_MACROS_H_
