// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PARAM_TRAITS_READ_MACROS_H_
#define IPC_PARAM_TRAITS_READ_MACROS_H_

// Null out all the macros that need nulling.
#include "ipc/ipc_message_null_macros.h"

// STRUCT declarations cause corresponding STRUCT_TRAITS declarations to occur.
#undef IPC_STRUCT_BEGIN
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN(struct_name) IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name) IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_END() IPC_STRUCT_TRAITS_END()

// Set up so next include will generate read methods.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  bool ParamTraits<struct_name>:: \
      Read(const Message* m, void** iter, param_type* p) { \
    return
#define IPC_STRUCT_TRAITS_MEMBER(name) ReadParam(m, iter, &p->name) &&
#define IPC_STRUCT_TRAITS_END() 1; }

#undef IPC_ENUM_TRAITS
#define IPC_ENUM_TRAITS(enum_name) \
  bool ParamTraits<enum_name>:: \
      Read(const Message* m, void** iter, param_type* p) { \
    int type; \
    if (!m->ReadInt(iter, &type)) \
      return false; \
    *p = static_cast<param_type>(type); \
    return true; \
  }

#endif  // IPC_PARAM_TRAITS_READ_MACROS_H_

