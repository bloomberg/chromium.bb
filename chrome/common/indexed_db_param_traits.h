// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INDEXED_DB_PARAM_TRAITS_H_
#define CHROME_COMMON_INDEXED_DB_PARAM_TRAITS_H_
#pragma once

#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"

class IndexedDBKey;
class SerializedScriptValue;

namespace IPC {

// These datatypes are used by utility_messages.h and render_messages.h.
// Unfortunately we can't move it to common: MSVC linker complains about
// WebKit datatypes that are not linked on npchrome_frame (even though it's
// never actually used by that target).

template <>
struct ParamTraits<SerializedScriptValue> {
  typedef SerializedScriptValue param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<IndexedDBKey> {
  typedef IndexedDBKey param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

} // namespace IPC

#endif  // CHROME_COMMON_INDEXED_DB_PARAM_TRAITS_H_
