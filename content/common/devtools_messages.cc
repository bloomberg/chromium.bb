// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/devtools_messages.h"

namespace IPC {

void ParamTraits<content::ConsoleMessageLevel>::Write(Message* m,
                                                      const param_type& p) {
  m->WriteInt(static_cast<int>(p));
}

bool ParamTraits<content::ConsoleMessageLevel>::Read(const Message* m,
                                                     PickleIterator* iter,
                                                     param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<param_type>(type);
  return true;
}

void ParamTraits<content::ConsoleMessageLevel>::Log(const param_type& p,
                                                    std::string* l) {
  std::string level;
  switch (p) {
    case content::CONSOLE_MESSAGE_LEVEL_TIP:
      level = "CONSOLE_MESSAGE_LEVEL_TIP";
      break;
    case content::CONSOLE_MESSAGE_LEVEL_LOG:
      level = "CONSOLE_MESSAGE_LEVEL_LOG";
      break;
    case content::CONSOLE_MESSAGE_LEVEL_WARNING:
      level = "CONSOLE_MESSAGE_LEVEL_WARNING";
      break;
    case content::CONSOLE_MESSAGE_LEVEL_ERROR:
      level = "CONSOLE_MESSAGE_LEVEL_ERROR";
      break;
  }
  LogParam(level, l);
}

}  // namespace IPC
