// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PARAM_TRAITS_MACROS_H_
#define IPC_PARAM_TRAITS_MACROS_H_

#include <string>

// Traits generation for structs.
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  namespace IPC { \
    template <> \
    struct ParamTraits<struct_name> { \
      typedef struct_name param_type; \
      static void Write(Message* m, const param_type& p); \
      static bool Read(const Message* m, void** iter, param_type* p); \
      static void Log(const param_type& p, std::string* l); \
    }; \
  }

#define IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_TRAITS_PARENT(type)
#define IPC_STRUCT_TRAITS_END()

// Traits generation for enums.
#define IPC_ENUM_TRAITS(enum_name) \
  namespace IPC { \
    template <> \
    struct ParamTraits<enum_name> { \
      typedef enum_name param_type; \
      static void Write(Message* m, const param_type& p); \
      static bool Read(const Message* m, void** iter, param_type* p); \
      static void Log(const param_type& p, std::string* l); \
    }; \
  }

#endif  // IPC_PARAM_TRAITS_MACROS_H_

