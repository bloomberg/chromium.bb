// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webkit_param_traits.h"

#include "base/format_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"

namespace IPC {

void ParamTraits<WebKit::WebRect>::Write(Message* m, const param_type& p) {
    WriteParam(m, p.x);
    WriteParam(m, p.y);
    WriteParam(m, p.width);
    WriteParam(m, p.height);
  }

bool ParamTraits<WebKit::WebRect>::Read(const Message* m, void** iter,
                                        param_type* p) {
  return
      ReadParam(m, iter, &p->x) &&
      ReadParam(m, iter, &p->y) &&
      ReadParam(m, iter, &p->width) &&
      ReadParam(m, iter, &p->height);
}

void ParamTraits<WebKit::WebRect>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.x, l);
  l->append(", ");
  LogParam(p.y, l);
  l->append(", ");
  LogParam(p.width, l);
  l->append(", ");
  LogParam(p.height, l);
  l->append(")");
}

void ParamTraits<WebKit::WebScreenInfo>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.depth);
  WriteParam(m, p.depthPerComponent);
  WriteParam(m, p.isMonochrome);
  WriteParam(m, p.rect);
  WriteParam(m, p.availableRect);
}

bool ParamTraits<WebKit::WebScreenInfo>::Read(const Message* m, void** iter,
                                              param_type* p) {
  return
      ReadParam(m, iter, &p->depth) &&
      ReadParam(m, iter, &p->depthPerComponent) &&
      ReadParam(m, iter, &p->isMonochrome) &&
      ReadParam(m, iter, &p->rect) &&
      ReadParam(m, iter, &p->availableRect);
}

void ParamTraits<WebKit::WebScreenInfo>::Log(const param_type& p,
                                             std::string* l) {
  l->append("(");
  LogParam(p.depth, l);
  l->append(", ");
  LogParam(p.depthPerComponent, l);
  l->append(", ");
  LogParam(p.isMonochrome, l);
  l->append(", ");
  LogParam(p.rect, l);
  l->append(", ");
  LogParam(p.availableRect, l);
  l->append(")");
}

void ParamTraits<WebKit::WebCache::ResourceTypeStat>::Log(
    const param_type& p, std::string* l) {
  l->append(base::StringPrintf("%" PRIuS " %" PRIuS " %" PRIuS " %" PRIuS,
                               p.count, p.size, p.liveSize, p.decodedSize));
}

void ParamTraits<WebKit::WebTextCheckingResult>::Write(Message* m,
                                                       const param_type& p) {
#if defined(WEB_TEXT_CHECKING_RESULT_IS_A_STRUCT)
  WriteParam(m, static_cast<int>(p.error));
  WriteParam(m, p.position);
  WriteParam(m, p.length);
#else
  WriteParam(m, static_cast<int>(p.error()));
  WriteParam(m, p.position());
  WriteParam(m, p.length());
#endif
}

bool ParamTraits<WebKit::WebTextCheckingResult>::Read(const Message* m,
                                                      void** iter,
                                                      param_type* p) {
  int error = 0;
  if (!ReadParam(m, iter, &error))
    return false;
  if (error != WebKit::WebTextCheckingResult::ErrorSpelling &&
      error != WebKit::WebTextCheckingResult::ErrorGrammar)
    return false;
  int position = 0;
  if (!ReadParam(m, iter, &position))
    return false;
  int length = 0;
  if (!ReadParam(m, iter, &length))
    return false;

  *p = WebKit::WebTextCheckingResult(
      static_cast<WebKit::WebTextCheckingResult::Error>(error),
      position,
      length);
  return true;
}

void ParamTraits<WebKit::WebTextCheckingResult>::Log(const param_type& p,
                                                     std::string* l) {
  l->append("(");
#if defined(WEB_TEXT_CHECKING_RESULT_IS_A_STRUCT)
  LogParam(static_cast<int>(p.error), l);
  l->append(", ");
  LogParam(p.position, l);
  l->append(", ");
  LogParam(p.length, l);
#else
  LogParam(static_cast<int>(p.error()), l);
  l->append(", ");
  LogParam(p.position(), l);
  l->append(", ");
  LogParam(p.length(), l);
#endif
  l->append(")");
}

}  // namespace IPC
