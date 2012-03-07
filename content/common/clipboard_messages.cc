// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/clipboard_messages.h"

#include "ui/base/clipboard/clipboard.h"

namespace IPC {

void ParamTraits<ui::Clipboard::FormatType>::Write(
    Message* m, const param_type& p) {
  m->WriteString(p.Serialize());
}

bool ParamTraits<ui::Clipboard::FormatType>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  std::string serialization;
  if (!ReadParam(m, iter, &serialization))
    return false;
  *r = ui::Clipboard::FormatType::Deserialize(serialization);
  return true;
}

void ParamTraits<ui::Clipboard::FormatType>::Log(
    const param_type& p, std::string* l) {
  *l = p.Serialize();
}

}  // namespace IPC
