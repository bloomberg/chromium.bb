// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_DEVTOOLS_MESSAGES_H_
#define CHROME_COMMON_DEVTOOLS_MESSAGES_H_

#include "ipc/ipc_message_utils.h"
#include "webkit/glue/devtools/devtools_message_data.h"

namespace IPC {

// Traits for DevToolsMessageData structure to pack/unpack.
template <>
struct ParamTraits<DevToolsMessageData> {
  typedef DevToolsMessageData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.class_name);
    WriteParam(m, p.method_name);
    WriteParam(m, p.arguments);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->class_name) &&
        ReadParam(m, iter, &p->method_name) &&
        ReadParam(m, iter, &p->arguments);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<DevToolsMessageData>");
  }
};

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/devtools_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_DEVTOOLS_MESSAGES_H_
