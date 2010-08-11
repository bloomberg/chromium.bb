// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"

#include "chrome/common/thumbnail_score.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webcursor.h"

#define MESSAGES_INTERNAL_IMPL_FILE \
  "chrome/common/render_messages_internal.h"
#include "ipc/ipc_message_impl_macros.h"

namespace IPC {

void ParamTraits<webkit_glue::WebAccessibility>::Write(Message* m,
                                                       const param_type& p) {
  WriteParam(m, p.id);
  WriteParam(m, p.name);
  WriteParam(m, p.value);
  WriteParam(m, static_cast<int>(p.role));
  WriteParam(m, static_cast<int>(p.state));
  WriteParam(m, p.location);
  WriteParam(m, p.attributes);
  WriteParam(m, p.children);
}

bool ParamTraits<webkit_glue::WebAccessibility>::Read(
    const Message* m, void** iter, param_type* p) {
  bool ret = ReadParam(m, iter, &p->id);
  ret = ret && ReadParam(m, iter, &p->name);
  ret = ret && ReadParam(m, iter, &p->value);
  int role = -1;
  ret = ret && ReadParam(m, iter, &role);
  if (role >= webkit_glue::WebAccessibility::ROLE_NONE &&
      role < webkit_glue::WebAccessibility::NUM_ROLES) {
    p->role = static_cast<webkit_glue::WebAccessibility::Role>(role);
  } else {
    p->role = webkit_glue::WebAccessibility::ROLE_NONE;
  }
  int state = 0;
  ret = ret && ReadParam(m, iter, &state);
  p->state = static_cast<webkit_glue::WebAccessibility::State>(state);
  ret = ret && ReadParam(m, iter, &p->location);
  ret = ret && ReadParam(m, iter, &p->attributes);
  ret = ret && ReadParam(m, iter, &p->children);
  return ret;
}

void ParamTraits<webkit_glue::WebAccessibility>::Log(const param_type& p,
                                                     std::wstring* l) {
  l->append(L"(");
  LogParam(p.id, l);
  l->append(L", ");
  LogParam(p.name, l);
  l->append(L", ");
  LogParam(p.value, l);
  l->append(L", ");
  LogParam(static_cast<int>(p.role), l);
  l->append(L", ");
  LogParam(static_cast<int>(p.state), l);
  l->append(L", ");
  LogParam(p.location, l);
  l->append(L", ");
  LogParam(p.attributes, l);
  l->append(L", ");
  LogParam(p.children, l);
  l->append(L")");
}

}  // namespace IPC
