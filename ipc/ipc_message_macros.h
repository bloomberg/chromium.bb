// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defining IPC Messages
//
// Your IPC messages will be defined by macros inside of an XXX_messages.h
// header file.  Most of the time, the system can automatically generate all
// of messaging mechanism from these definitions, but sometimes some manual
// coding is required.  In these cases, you will also have an XXX_messages.cc
// implemation file as well.
//
// The senders of your messages will include your XXX_messages.h file to
// get the full set of definitions they need to send your messages.
//
// Each XXX_messages.h file must be registered with the IPC system.  This
// requires adding two things:
//   - An XXXMsgStart value to the IPCMessageStart enum in ipc_message_utils.h
//   - An inclusion of XXX_messages.h file in a message generator .h file
//
// The XXXMsgStart value is an enumeration that ensures uniqueness for
// each different message file.  Later, you will use this inside your
// XXX_messages.h file before invoking message declatation macros:
//     #define IPC_MESSAGE_START XXXMsgStart
//       ( ... your macro invocations go here ... )
//
// Message Generator Files
//
// A message generator .h header file pulls in all other message-declaring
// headers for a given component.  It is included by a message generator
// .cc file, which is where all the generated code will wind up.  Typically,
// you will use an existing generator (e.g. common_message_generator.cc and
// common_message_generator.h in /chrome/common), but there are circumstances
// where you may add a new one.
//
// In the rare cicrucmstances where you can't re-use an existing file,
// your YYY_message_generator.cc file for a component YYY would contain
// the following code:
//     // Get basic type definitions.
//     #define IPC_MESSAGE_IMPL
//     #include "path/to/YYY_message_generator.h"
//     // Generate constructors.
//     #include "ipc/struct_constructor_macros.h"
//     #include "path/to/YYY_message_generator.h"
//     // Generate destructors.
//     #include "ipc/struct_destructor_macros.h"
//     #include "path/to/YYY_message_generator.h"
//     namespace IPC {
//     // Generate param traits write methods.
//     #include "ipc/param_traits_write_macros.h"
//     #include "path/to/YYY_message_generator.h"
//     // Generate param traits read methods.
//     #include "ipc/param_traits_read_macros.h"
//     #include "path/to/YYY_message_generator.h"
//     // Generate param traits log methods.
//     #include "ipc/param_traits_log_macros.h"
//     #include "path/to/YYY_message_generator.h"
//     }  // namespace IPC
//
// In cases where manual generation is required, in your XXX_messages.cc
// file, put the following after all the includes for param types:
//     #define IPC_MESSAGE_IMPL
//     #include "XXX_messages.h"
//        (... implementation of traits not auto-generated ...)
//
// Multiple Inclusion
//
// The XXX_messages.h file will be multiply-included by the
// YYY_message_generator.cc file, so your XXX_messages file can't be
// guarded in the usual manner.  Ideally, there will be no need for any
// inclusion guard, since the XXX_messages.h file should consist soley
// of inclusions of other headers (which are self-guarding) and IPC
// macros (which are multiply evaluating).
//
// Note that there is no #pragma once either; doing so would mark the whole
// file as being singly-included.  Since your XXX_messages.h file is only
// partially-guarded, care must be taken to ensure that it is only included
// by other .cc files (and the YYY_message_generator.h file).  Including an
// XXX_messages.h file in some other .h file may result in duplicate
// declarations and a compilation failure.
//
// Type Declarations
//
// It is generally a bad idea to have type definitions in a XXX_messages.h
// file; most likely the typedef will then be used in the message, as opposed
// to the struct iself.  Later, an IPC message dispatcher wil need to call
// a function taking that type, and that function is declared in some other
// header.  Thus, in order to get the type definition, the other header
// would have to include the XXX_messages.h file, violating the rule above
// about not including XXX_messages.h file in other .h files.
//
// One approach here is to move these type definitions to another (guarded)
// .h file and include this second .h in your XXX_messages.h file.  This
// is still less than ideal, because the dispatched function would have to
// redeclare the typedef or include this second header.  This may be
// reasonable in a few cases.
//
// Failing all of the above, then you will want to bracket the smallest
// possible section of your XXX_messages.h file containing these types
// with an include guard macro.  Be aware that providing an incomplete
// class type declaration to avoid pulling in a long chain of headers is
// acceptable when your XXX_messages.h header is being included by the
// message sending caller's code, but not when the YYY_message_generator.c
// is building the messages. In addtion, due to the multiple inclusion
// restriction, these type ought to be guarded.  Follow a convention like:
//      #ifndef SOME_GUARD_MACRO
//      #define SOME_GUARD_MACRO
//      class some_class;        // One incomplete class declaration
//      class_some_other_class;  // Another incomplete class declaration
//      #endif  // SOME_GUARD_MACRO
//      #ifdef IPC_MESSAGE_IMPL
//      #inlcude "path/to/some_class.h"        // Full class declaration
//      #inlcude "path/to/some_other_class.h"  // Full class declaration
//      #endif  // IPC_MESSAGE_IMPL
//        (.. IPC macros using some_class and some_other_class ...)
//
// Macro Invocations
//
// You will use IPC message macro invocations for three things:
//   - New struct definitions for IPC
//   - Registering existing struct and enum definitions with IPC
//   - Defining the messages themselves
//
// New structs are defined with IPC_STRUCT_BEGIN(), IPC_STRUCT_MEMBER(),
// IPC_STRUCT_END() family of macros.  These cause the XXX_messages.h
// to proclaim equivalent struct declarations for use by callers, as well
// as later registering the type with the message generation.  Note that
// IPC_STRUCT_MEMBER() is only permitted inside matching calls to
// IPC_STRUCT_BEGIN() / IPC_STRUCT_END().
//
// Externally-defined structs are registered with IPC_STRUCT_TRAITS_BEGIN(),
// IPC_STRUCT_TRAITS_MEMBER(), and IPC_STRUCT_TRAITS_END() macros. These
// cause registration of the types with message generation only.  Note that
// IPC_STRUCT_TRAITS_MEMBER() is only permitted inside matching calls
// to IPC_STRUCT_TRAITS_BEGIN() / IPC_STRUCT_TRAITS_END().
//
// Enum types are registered with a single IPC_ENUM_TRAITS() macro.  There
// is no need to enumerate each value to the IPC mechanism.
//
// Once the types have been declared / registered, message definitions follow.
// "Sync" messages are just synchronous calls, the Send() call doesn't return
// until a reply comes back.  Input parameters are first (const TYPE&), and
// To declare a sync message, use the IPC_SYNC_ macros.  The numbers at the
// end show how many input/output parameters there are (i.e. 1_2 is 1 in, 2
// out). The caller does a Send([route id, ], in1, &out1, &out2).
// The receiver's handler function will be
//     void OnSyncMessageName(const type1& in1, type2* out1, type3* out2)
//
// A caller can also send a synchronous message, while the receiver can respond
// at a later time.  This is transparent from the sender's side.  The receiver
// needs to use a different handler that takes in a IPC::Message* as the output
// type, stash the message, and when it has the data it can Send the message.
//
// Use the IPC_MESSAGE_HANDLER_DELAY_REPLY macro instead of IPC_MESSAGE_HANDLER
//     IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SyncMessageName,
//                                     OnSyncMessageName)
//
// The handler function will look like:
//     void OnSyncMessageName(const type1& in1, IPC::Message* reply_msg);
//
// Receiver stashes the IPC::Message* pointer, and when it's ready, it does:
//     ViewHostMsg_SyncMessageName::WriteReplyParams(reply_msg, out1, out2);
//     Send(reply_msg);

#ifndef IPC_IPC_MESSAGE_MACROS_H_
#define IPC_IPC_MESSAGE_MACROS_H_
// Can use #pragma once all XXX_messages.h files clean up IPC_MESSAGE_START

#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"

#if defined(IPC_MESSAGE_IMPL)
#include "ipc/ipc_message_impl_macros.h"
#elif defined(IPC_MESSAGE_MACROS_LOG_ENABLED)

#ifndef IPC_LOG_TABLE_CREATED
#define IPC_LOG_TABLE_CREATED

#include "base/hash_tables.h"

typedef void (*LogFunction)(std::string* name,
                            const IPC::Message* msg,
                            std::string* params);

typedef base::hash_map<uint32, LogFunction > LogFunctionMap;
LogFunctionMap g_log_function_mapping;

#endif  // IPC_LOG_TABLE_CREATED


#define IPC_MESSAGE_LOG(msg_class) \
  class LoggerRegisterHelper##msg_class { \
   public: \
    LoggerRegisterHelper##msg_class() { \
      g_log_function_mapping[msg_class::ID] = msg_class::Log; \
    } \
  }; \
  LoggerRegisterHelper##msg_class g_LoggerRegisterHelper##msg_class;

#define IPC_MESSAGE_CONTROL0_EXTRA(msg_class) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL1_EXTRA(msg_class, type1) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL2_EXTRA(msg_class, type1, type2) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL3_EXTRA(msg_class, type1, type2, type3) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL4_EXTRA(msg_class, type1, type2, type3, type4) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL5_EXTRA(msg_class, type1, type2, type3, type4, type5) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED0_EXTRA(msg_class) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED1_EXTRA(msg_class, type1) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED2_EXTRA(msg_class, type1, type2) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED3_EXTRA(msg_class, type1, type2, type3) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED4_EXTRA(msg_class, type1, type2, type3, type4) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED5_EXTRA(msg_class, type1, type2, type3, type4, type5) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_0_EXTRA(msg_class) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_1_EXTRA(msg_class, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_2_EXTRA(msg_class, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_3_EXTRA(msg_class, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_4_EXTRA(msg_class, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_0_EXTRA(msg_class, type1_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_1_EXTRA(msg_class, type1_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_2_EXTRA(msg_class, type1_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_3_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_4_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_0_EXTRA(msg_class, type1_in, type2_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_1_EXTRA(msg_class, type1_in, type2_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_2_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_3_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_4_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL4_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL4_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL4_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL4_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL5_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL5_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL5_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL5_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_0_EXTRA(msg_class) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_1_EXTRA(msg_class, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_2_EXTRA(msg_class, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_3_EXTRA(msg_class, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_4_EXTRA(msg_class, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_0_EXTRA(msg_class, type1_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_1_EXTRA(msg_class, type1_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_2_EXTRA(msg_class, type1_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_3_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_4_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_0_EXTRA(msg_class, type1_in, type2_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_1_EXTRA(msg_class, type1_in, type2_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_2_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_3_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_4_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_0_EXTRA(msg_class, type1_in, type2_in, type3_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_0_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)


#define IPC_SYNC_MESSAGE_ROUTED4_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_0_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED5_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#else  // defined(IPC_MESSAGE_MACROS_LOG_ENABLED)

#define IPC_MESSAGE_CONTROL0_EXTRA(msg_class)
#define IPC_MESSAGE_CONTROL1_EXTRA(msg_class, type1)
#define IPC_MESSAGE_CONTROL2_EXTRA(msg_class, type1, type2)
#define IPC_MESSAGE_CONTROL3_EXTRA(msg_class, type1, type2, type3)
#define IPC_MESSAGE_CONTROL4_EXTRA(msg_class, type1, type2, type3, type4)
#define IPC_MESSAGE_CONTROL5_EXTRA(msg_class, type1, type2, type3, type4, type5)
#define IPC_MESSAGE_ROUTED0_EXTRA(msg_class)
#define IPC_MESSAGE_ROUTED1_EXTRA(msg_class, type1)
#define IPC_MESSAGE_ROUTED2_EXTRA(msg_class, type1, type2)
#define IPC_MESSAGE_ROUTED3_EXTRA(msg_class, type1, type2, type3)
#define IPC_MESSAGE_ROUTED4_EXTRA(msg_class, type1, type2, type3, type4)
#define IPC_MESSAGE_ROUTED5_EXTRA(msg_class, type1, type2, type3, type4, type5)
#define IPC_SYNC_MESSAGE_CONTROL0_0_EXTRA(msg_class)
#define IPC_SYNC_MESSAGE_CONTROL0_1_EXTRA(msg_class, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL0_2_EXTRA(msg_class, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL0_3_EXTRA(msg_class, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL0_4_EXTRA(msg_class, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL1_0_EXTRA(msg_class, type1_in)
#define IPC_SYNC_MESSAGE_CONTROL1_1_EXTRA(msg_class, type1_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL1_2_EXTRA(msg_class, type1_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL1_3_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL1_4_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL2_0_EXTRA(msg_class, type1_in, type2_in)
#define IPC_SYNC_MESSAGE_CONTROL2_1_EXTRA(msg_class, type1_in, type2_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL2_2_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL2_3_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL2_4_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL3_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL3_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL3_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL3_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL4_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL4_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL4_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL4_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL5_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL5_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL5_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL5_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED0_0_EXTRA(msg_class)
#define IPC_SYNC_MESSAGE_ROUTED0_1_EXTRA(msg_class, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED0_2_EXTRA(msg_class, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED0_3_EXTRA(msg_class, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED0_4_EXTRA(msg_class, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED1_0_EXTRA(msg_class, type1_in)
#define IPC_SYNC_MESSAGE_ROUTED1_1_EXTRA(msg_class, type1_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED1_2_EXTRA(msg_class, type1_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED1_3_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED1_4_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED2_0_EXTRA(msg_class, type1_in, type2_in)
#define IPC_SYNC_MESSAGE_ROUTED2_1_EXTRA(msg_class, type1_in, type2_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED2_2_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED2_3_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED2_4_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED3_0_EXTRA(msg_class, type1_in, type2_in, type3_in)
#define IPC_SYNC_MESSAGE_ROUTED3_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED3_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED3_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED3_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED4_0_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in)
#define IPC_SYNC_MESSAGE_ROUTED4_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED4_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED4_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED4_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED5_0_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in)
#define IPC_SYNC_MESSAGE_ROUTED5_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED5_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED5_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED5_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type4_out)

#endif  // defined(IPC_MESSAGE_MACROS_LOG_ENABLED)

// Macros for defining structs.  May be subsequently redefined.
#define IPC_STRUCT_BEGIN(struct_name) \
  struct struct_name; \
  IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  IPC_STRUCT_TRAITS_END() \
  struct struct_name : IPC::NoParams { \
    struct_name(); \
    ~struct_name();
#define IPC_STRUCT_MEMBER(type, name) type name;
#define IPC_STRUCT_END() };

// Note: we currently use __LINE__ to give unique IDs to messages within a file.
// They're globally unique since each file defines its own IPC_MESSAGE_START.
// Ideally, we wouldn't use line numbers, but instead use the __COUNTER__ macro,
// but it needs gcc 4.3 and xcode doesn't use it yet.  When that happens, switch
// to it.

#define IPC_MESSAGE_CONTROL0(msg_class) \
  class msg_class : public IPC::Message { \
   public: \
   enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class() \
        : IPC::Message(MSG_ROUTING_CONTROL, \
                       ID, \
                       PRIORITY_NORMAL) {} \
  }; \
  IPC_MESSAGE_CONTROL0_EXTRA(msg_class)

#define IPC_MESSAGE_CONTROL1(msg_class, type1) \
  class msg_class : public IPC::MessageWithTuple< Tuple1<type1> > { \
   public:                                                          \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };             \
    msg_class(const type1& arg1);                                   \
    ~msg_class();                                                   \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_CONTROL1_EXTRA(msg_class, type1)

#define IPC_MESSAGE_CONTROL2(msg_class, type1, type2)                   \
  class msg_class : public IPC::MessageWithTuple< Tuple2<type1, type2> > { \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(const type1& arg1, const type2& arg2);                    \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_CONTROL2_EXTRA(msg_class, type1, type2)

#define IPC_MESSAGE_CONTROL3(msg_class, type1, type2, type3)            \
  class msg_class :                                                     \
      public IPC::MessageWithTuple< Tuple3<type1, type2, type3> > {     \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(const type1& arg1, const type2& arg2, const type3& arg3); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_CONTROL3_EXTRA(msg_class, type1, type2, type3)

#define IPC_MESSAGE_CONTROL4(msg_class, type1, type2, type3, type4)     \
  class msg_class :                                                     \
      public IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> > { \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(const type1& arg1, const type2& arg2, const type3& arg3,  \
              const type4& arg4);                                       \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_CONTROL4_EXTRA(msg_class, type1, type2, type3, type4)

#define IPC_MESSAGE_CONTROL5(msg_class, type1, type2, type3, type4, type5) \
  class msg_class :                                                     \
      public IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4,  \
                                           type5> > { \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(const type1& arg1, const type2& arg2,                     \
              const type3& arg3, const type4& arg4, const type5& arg5); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_CONTROL5_EXTRA(msg_class, type1, type2, type3, type4, type5)

#define IPC_MESSAGE_ROUTED0(msg_class) \
  class msg_class : public IPC::Message { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int32 routing_id) \
        : IPC::Message(routing_id, ID, PRIORITY_NORMAL) {} \
  }; \
  IPC_MESSAGE_ROUTED0_EXTRA(msg_class)

#define IPC_MESSAGE_ROUTED1(msg_class, type1) \
  class msg_class : public IPC::MessageWithTuple< Tuple1<type1> > {     \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(int32 routing_id, const type1& arg1);                     \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_ROUTED1_EXTRA(msg_class, type1)

#define IPC_MESSAGE_ROUTED2(msg_class, type1, type2)                    \
  class msg_class                                                       \
      : public IPC::MessageWithTuple< Tuple2<type1, type2> > {          \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(int32 routing_id, const type1& arg1, const type2& arg2);  \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_ROUTED2_EXTRA(msg_class, type1, type2)

#define IPC_MESSAGE_ROUTED3(msg_class, type1, type2, type3)             \
  class msg_class                                                       \
      : public IPC::MessageWithTuple< Tuple3<type1, type2, type3> > {   \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(int32 routing_id, const type1& arg1, const type2& arg2,   \
              const type3& arg3);                                       \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_ROUTED3_EXTRA(msg_class, type1, type2, type3)

#define IPC_MESSAGE_ROUTED4(msg_class, type1, type2, type3, type4)      \
  class msg_class                                                       \
      : public IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> > { \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(int32 routing_id, const type1& arg1, const type2& arg2,   \
              const type3& arg3, const type4& arg4);                    \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_ROUTED4_EXTRA(msg_class, type1, type2, type3, type4)

#define IPC_MESSAGE_ROUTED5(msg_class, type1, type2, type3, type4, type5) \
  class msg_class                                                       \
      : public IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, \
                                             type5> > {                 \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(int32 routing_id, const type1& arg1, const type2& arg2,   \
              const type3& arg3, const type4& arg4, const type5& arg5); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_MESSAGE_ROUTED5_EXTRA(msg_class, type1, type2, type3, type4, type5)

#define IPC_SYNC_MESSAGE_CONTROL0_0(msg_class) \
  class msg_class : public IPC::MessageWithReply<Tuple0, Tuple0 > { \
   public:                                                          \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };             \
    msg_class();                                                    \
    ~msg_class();                                                   \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL0_0_EXTRA(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_1(msg_class, type1_out)               \
  class msg_class : public IPC::MessageWithReply<Tuple0, Tuple1<type1_out&> > {\
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(type1_out* arg1);                                         \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL0_1_EXTRA(msg_class, type1_out)

#define IPC_SYNC_MESSAGE_CONTROL0_2(msg_class, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                  \
    msg_class(type1_out* arg1, type2_out* arg2);                         \
    ~msg_class();                                                        \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL0_2_EXTRA(msg_class, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_CONTROL0_3(msg_class, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, \
          Tuple3<type1_out&, type2_out&, type3_out&> >{ \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(type1_out* arg1, type2_out* arg2, type3_out* arg3);        \
    ~msg_class();                                                        \
    static void Log(std::string* name, const Message* msg, std::string* l);  \
  }; \
  IPC_SYNC_MESSAGE_CONTROL0_3_EXTRA(msg_class, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_CONTROL0_4(msg_class, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >{ \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(type1_out* arg1, type2_out* arg2, type3_out* arg3, type4_out* arg4); \
    ~msg_class();                                                        \
    static void Log(std::string* name, const Message* msg, std::string* l);  \
  }; \
  IPC_SYNC_MESSAGE_CONTROL0_4_EXTRA(msg_class, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_CONTROL1_0(msg_class, type1_in)                \
  class msg_class :                                                     \
      public IPC::MessageWithReply<Tuple1<type1_in>, Tuple0 > {         \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(const type1_in& arg1);                                    \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL1_0_EXTRA(msg_class, type1_in)

#define IPC_SYNC_MESSAGE_CONTROL1_1(msg_class, type1_in, type1_out)     \
  class msg_class :                                                     \
      public IPC::MessageWithReply<Tuple1<type1_in>, Tuple1<type1_out&> > { \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(const type1_in& arg1, type1_out* arg2);                   \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL1_1_EXTRA(msg_class, type1_in, type1_out)

#define IPC_SYNC_MESSAGE_CONTROL1_2(msg_class, type1_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple1<type1_in>, Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, type1_out* arg2, type2_out* arg3);  \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL1_2_EXTRA(msg_class, type1_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_CONTROL1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple1<type1_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> >{ \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, type1_out* arg2, type2_out* arg3, type3_out* arg4); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL1_3_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_CONTROL1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple1<type1_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >{ \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, type1_out* arg2, type2_out* arg3, type3_out* arg4, type4_out* arg5); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL1_4_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_CONTROL2_0(msg_class, type1_in, type2_in)      \
  class msg_class :                                                     \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple0 > { \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(const type1_in& arg1, const type2_in& arg2);              \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL2_0_EXTRA(msg_class, type1_in, type2_in)

#define IPC_SYNC_MESSAGE_CONTROL2_1(msg_class, type1_in, type2_in, type1_out) \
  class msg_class :                                                     \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple1<type1_out&> > { \
   public:                                                              \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ };                 \
    msg_class(const type1_in& arg1, const type2_in& arg2, type1_out* arg3); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL2_1_EXTRA(msg_class, type1_in, type2_in, type1_out)

#define IPC_SYNC_MESSAGE_CONTROL2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL2_2_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_CONTROL2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4, type3_out* arg5); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL2_3_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_CONTROL2_4(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4, type3_out* arg5, type4_out* arg6); \
    ~msg_class();                                                       \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL2_4_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_CONTROL3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4); \
    ~msg_class();                                                      \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL3_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out)

#define IPC_SYNC_MESSAGE_CONTROL3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL3_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_CONTROL3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5, type3_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL3_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_CONTROL3_4(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5, type3_out* arg6, type4_out* arg7); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l);             \
  }; \
  IPC_SYNC_MESSAGE_CONTROL3_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_CONTROL4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL4_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out)

#define IPC_SYNC_MESSAGE_CONTROL4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg5, type2_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL4_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_CONTROL4_3(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg5, type2_out* arg6, type3_out* arg7); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL4_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_CONTROL4_4(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg5, type2_out* arg6, type3_out* arg7, type4_out* arg8); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL4_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_CONTROL5_1(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type5_in& arg5, type1_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL5_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out)

#define IPC_SYNC_MESSAGE_CONTROL5_2(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type5_in& arg5, type1_out* arg6, type2_out* arg7); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL5_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_CONTROL5_3(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type5_in& arg5, type1_out* arg6, type2_out* arg7, type3_out* arg8); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL5_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_CONTROL5_4(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type5_in& arg5, type1_out* arg6, type2_out* arg7, type3_out* arg8, type4_out* arg9); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_CONTROL5_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_ROUTED0_0(msg_class) \
  class msg_class : public IPC::MessageWithReply<Tuple0, Tuple0 > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id);     \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED0_0_EXTRA(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_1(msg_class, type1_out) \
  class msg_class : public IPC::MessageWithReply<Tuple0, Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, type1_out* arg1);  \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l);             \
  }; \
  IPC_SYNC_MESSAGE_ROUTED0_1_EXTRA(msg_class, type1_out)

#define IPC_SYNC_MESSAGE_ROUTED0_2(msg_class, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, type1_out* arg1, type2_out* arg2); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED0_2_EXTRA(msg_class, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_ROUTED0_3(msg_class, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, \
          Tuple3<type1_out&, type2_out&, type3_out&> >{ \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, type1_out* arg1, type2_out* arg2, type3_out* arg3); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED0_3_EXTRA(msg_class, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_ROUTED0_4(msg_class, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >{ \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, type1_out* arg1, type2_out* arg2, type3_out* arg3, type4_out* arg4); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED0_4_EXTRA(msg_class, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_ROUTED1_0(msg_class, type1_in) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple1<type1_in>, Tuple0 > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED1_0_EXTRA(msg_class, type1_in)

#define IPC_SYNC_MESSAGE_ROUTED1_1(msg_class, type1_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple1<type1_in>, Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, type1_out* arg2);    \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED1_1_EXTRA(msg_class, type1_in, type1_out)

#define IPC_SYNC_MESSAGE_ROUTED1_2(msg_class, type1_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple1<type1_in>, Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, type1_out* arg2, type2_out* arg3); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED1_2_EXTRA(msg_class, type1_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_ROUTED1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple1<type1_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> >{ \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, type1_out* arg2, type2_out* arg3, type3_out* arg4); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED1_3_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_ROUTED1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple1<type1_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >{ \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, type1_out* arg2, type2_out* arg3, type3_out* arg4, type4_out* arg5); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED1_4_EXTRA(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_ROUTED2_0(msg_class, type1_in, type2_in) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple0 > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED2_0_EXTRA(msg_class, type1_in, type2_in)

#define IPC_SYNC_MESSAGE_ROUTED2_1(msg_class, type1_in, type2_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, type1_out* arg3); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED2_1_EXTRA(msg_class, type1_in, type2_in, type1_out)

#define IPC_SYNC_MESSAGE_ROUTED2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED2_2_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_ROUTED2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4, type3_out* arg5); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED2_3_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_ROUTED2_4(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4, type3_out* arg5, type4_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED2_4_EXTRA(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_ROUTED3_0(msg_class, type1_in, type2_in, type3_in) \
    class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, Tuple0 > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
    }; \
  IPC_SYNC_MESSAGE_ROUTED3_0_EXTRA(msg_class, type1_in, type2_in, type3_in)

#define IPC_SYNC_MESSAGE_ROUTED3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED3_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out)

#define IPC_SYNC_MESSAGE_ROUTED3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED3_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_ROUTED3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5, type3_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED3_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_ROUTED3_4(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5, type3_out* arg6, type4_out* arg7); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED3_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_ROUTED4_0(msg_class, type1_in, type2_in, type3_in, type4_in) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple0 > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED4_0_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in)

#define IPC_SYNC_MESSAGE_ROUTED4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED4_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out)

#define IPC_SYNC_MESSAGE_ROUTED4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg5, type2_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED4_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_ROUTED4_3(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg5, type2_out* arg6, type3_out* arg7); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED4_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_ROUTED4_4(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg5, type2_out* arg6, type3_out* arg7, type4_out* arg8); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED4_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out)

#define IPC_SYNC_MESSAGE_ROUTED5_0(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple0 > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type5_in& arg5); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED5_0_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in)

#define IPC_SYNC_MESSAGE_ROUTED5_1(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple1<type1_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type5_in& arg5, type1_out* arg6); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED5_1_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out)

#define IPC_SYNC_MESSAGE_ROUTED5_2(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type4_in& arg5, type1_out* arg6, type2_out* arg7); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED5_2_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out)

#define IPC_SYNC_MESSAGE_ROUTED5_3(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type4_in& arg5, type1_out* arg6, type2_out* arg7, type3_out* arg8); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED5_3_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out)

#define IPC_SYNC_MESSAGE_ROUTED5_4(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple5<type1_in, type2_in, type3_in, type4_in, type5_in>, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> > { \
   public: \
    enum { ID = (IPC_MESSAGE_START << 16) + __LINE__ }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, const type4_in& arg5, type1_out* arg6, type2_out* arg7, type3_out* arg8, type4_out* arg9); \
    ~msg_class();                                                     \
    static void Log(std::string* name, const Message* msg, std::string* l); \
  }; \
  IPC_SYNC_MESSAGE_ROUTED5_4_EXTRA(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out)



// Message crackers and handlers.
// Prefer to use the IPC_BEGIN_MESSAGE_MAP_EX to the older macros since they
// allow you to detect when a message could not be de-serialized. Usage:
//
//   bool MyClass::OnMessageReceived(const IPC::Message& msg) {
//     bool handled = true;
//     bool msg_is_good = false;
//     IPC_BEGIN_MESSAGE_MAP_EX(MyClass, msg, msg_is_good)
//       IPC_MESSAGE_HANDLER(MsgClassOne, OnMsgClassOne)
//       ...more handlers here ...
//       IPC_MESSAGE_HANDLER(MsgClassTen, OnMsgClassTen)
//       IPC_MESSAGE_UNHANDLED(handled = false)
//     IPC_END_MESSAGE_MAP_EX()
//     if (!msg_is_good) {
//       // Signal error here or terminate offending process.
//     }
//     return handled;
//   }


#define IPC_BEGIN_MESSAGE_MAP_EX(class_name, msg, msg_is_ok) \
  { \
    typedef class_name _IpcMessageHandlerClass; \
    const IPC::Message& ipc_message__ = msg; \
    bool& msg_is_ok__ = msg_is_ok; \
    switch (ipc_message__.type()) { \

#define IPC_BEGIN_MESSAGE_MAP(class_name, msg) \
  { \
    typedef class_name _IpcMessageHandlerClass; \
    const IPC::Message& ipc_message__ = msg; \
    bool msg_is_ok__ = true; \
    switch (ipc_message__.type()) { \

#define IPC_MESSAGE_FORWARD(msg_class, obj, member_func) \
  case msg_class::ID: \
    msg_is_ok__ = msg_class::Dispatch(&ipc_message__, obj, this, &member_func); \
    break;

#define IPC_MESSAGE_HANDLER(msg_class, member_func) \
  IPC_MESSAGE_FORWARD(msg_class, this, _IpcMessageHandlerClass::member_func)

#define IPC_MESSAGE_FORWARD_DELAY_REPLY(msg_class, obj, member_func) \
    case msg_class::ID: \
    msg_class::DispatchDelayReply(&ipc_message__, obj, &member_func); \
    break;

#define IPC_MESSAGE_HANDLER_DELAY_REPLY(msg_class, member_func) \
  IPC_MESSAGE_FORWARD_DELAY_REPLY(msg_class, this, \
                                  _IpcMessageHandlerClass::member_func)

#define IPC_MESSAGE_HANDLER_GENERIC(msg_class, code) \
  case msg_class::ID: \
    code; \
    break;

#define IPC_REPLY_HANDLER(func) \
   case IPC_REPLY_ID: \
     func(ipc_message__); \
     break;


#define IPC_MESSAGE_UNHANDLED(code) \
  default: \
    code; \
    break;

#define IPC_MESSAGE_UNHANDLED_ERROR() \
  IPC_MESSAGE_UNHANDLED(NOTREACHED() << \
                              "Invalid message with type = " << \
                              ipc_message__.type())

#define IPC_END_MESSAGE_MAP() \
    DCHECK(msg_is_ok__); \
  } \
}

#define IPC_END_MESSAGE_MAP_EX() \
  } \
}

// This corresponds to an enum value from IPCMessageStart.
#define IPC_MESSAGE_CLASS(message) \
  message.type() >> 16

#endif  // IPC_IPC_MESSAGE_MACROS_H_

// Clean up IPC_MESSAGE_START in this unguarded section so that the
// XXX_messages.h files need not do so themselves.  This makes the
// XXX_messages.h files easier to write.
#undef IPC_MESSAGE_START

