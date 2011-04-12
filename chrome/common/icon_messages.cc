// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/icon_messages.h"

#include "content/common/common_param_traits.h"

FaviconURL::FaviconURL()
  : icon_type(INVALID_ICON) {
}

FaviconURL::FaviconURL(const GURL& url, IconType type)
  : icon_url(url),
    icon_type(type) {
}

FaviconURL::~FaviconURL() {
}

namespace IPC {

// static
void ParamTraits<IconType>::Write(Message* m, const param_type& p) {
  m->WriteInt(p);
}

// static
bool ParamTraits<IconType>::Read(const Message* m, void** iter, param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<IconType>(type);
  return true;
}

// static
void ParamTraits<IconType>::Log(const param_type& p, std::string* l) {
  l->append("IconType");
}

// static
void ParamTraits<FaviconURL>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.icon_url);
  WriteParam(m, p.icon_type);
}

// static
bool ParamTraits<FaviconURL>::Read(const Message* m,
                                   void** iter,
                                   param_type* p) {
  return ReadParam(m, iter, &p->icon_url) && ReadParam(m, iter, &p->icon_type);
}

// static
void ParamTraits<FaviconURL>::Log(const param_type& p, std::string* l) {
  l->append("<FaviconURL>");
}

}  // namespace IPC
