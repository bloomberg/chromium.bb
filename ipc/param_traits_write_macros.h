// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PARAM_TRAITS_WRITE_MACROS_H_
#define IPC_PARAM_TRAITS_WRITE_MACROS_H_
#pragma once

// Null out all the macros that need nulling.
#include "ipc/ipc_message_null_macros.h"

// STRUCT declarations cause corresponding STRUCT_TRAITS declarations to occur.
#undef IPC_STRUCT_BEGIN
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN(struct_name) IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name) IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_END() IPC_STRUCT_TRAITS_END()

// Set up so next include will generate write methods.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  void ParamTraits<struct_name>::Write(Message* m, const param_type& p) {
#define IPC_STRUCT_TRAITS_MEMBER(name) WriteParam(m, p.name);
#define IPC_STRUCT_TRAITS_PARENT(type) ParamTraits<type>::Write(m, p);
#define IPC_STRUCT_TRAITS_END() }

#undef IPC_ENUM_TRAITS
#define IPC_ENUM_TRAITS(enum_name) \
  void ParamTraits<enum_name>::Write(Message* m, const param_type& p) { \
    m->WriteInt(static_cast<int>(p)); \
  }

#endif  // IPC_PARAM_TRAITS_WRITE_MACROS_H_

