// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_param_traits.h"

#include "content/common/content_param_traits.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"

namespace IPC {

void ParamTraits<content::ScopedWebInputEvent>::Write(Message* m,
                                                      const param_type& p) {
  bool valid_web_event = !!p;
  WriteParam(m, valid_web_event);
  if (valid_web_event)
    WriteParam(m, static_cast<WebInputEventPointer>(p.get()));
}

bool ParamTraits<content::ScopedWebInputEvent>::Read(const Message* m,
                                                     PickleIterator* iter,
                                                     param_type* p) {
  bool valid_web_event = false;
  WebInputEventPointer web_event_pointer = NULL;
  if (!ReadParam(m, iter, &valid_web_event) ||
      !valid_web_event ||
      !ReadParam(m, iter, &web_event_pointer) ||
      !web_event_pointer)
    return false;

  (*p) = content::WebInputEventTraits::Clone(*web_event_pointer);
  return true;
}

void ParamTraits<content::ScopedWebInputEvent>::Log(const param_type& p,
                                                    std::string* l) {
  LogParam(static_cast<WebInputEventPointer>(p.get()), l);
}

}  // namespace IPC
