// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to define IPC::ParamTraits<> specializations for a number
// of types so that they can be serialized over IPC.  IPC::ParamTraits<>
// specializations for basic types (like int and std::string) and types in the
// 'base' project can be found in ipc/ipc_message_utils.h.  This file contains
// specializations for types that are used by the content code, and which need
// manual serialization code.  This is usually because they're not structs with
// public members..

#ifndef CONTENT_COMMON_COMMON_PARAM_TRAITS_H_
#define CONTENT_COMMON_COMMON_PARAM_TRAITS_H_
#pragma once

#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_utils.h"
// !!! WARNING: DO NOT ADD NEW WEBKIT DEPENDENCIES !!!
//
// That means don't add #includes to any file in 'webkit/' or
// 'third_party/WebKit/'. Chrome Frame and NACL build parts of base/ and
// content/common/ for a mini-library that doesn't depend on webkit.

// Forward declarations.
class GURL;

namespace IPC {

template <>
struct ParamTraits<GURL> {
  typedef GURL param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_COMMON_PARAM_TRAITS_H_
