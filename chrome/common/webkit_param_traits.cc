// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webkit_param_traits.h"

#include "base/format_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerAction.h"
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

void ParamTraits<WebKit::WebFindOptions>::Write(Message* m,
                                                const param_type& p) {
  WriteParam(m, p.forward);
  WriteParam(m, p.matchCase);
  WriteParam(m, p.findNext);
}

bool ParamTraits<WebKit::WebFindOptions>::Read(const Message* m, void** iter,
                                               param_type* p) {
  return
      ReadParam(m, iter, &p->forward) &&
      ReadParam(m, iter, &p->matchCase) &&
      ReadParam(m, iter, &p->findNext);
}

void ParamTraits<WebKit::WebFindOptions>::Log(const param_type& p,
                                              std::string* l) {
  l->append("(");
  LogParam(p.forward, l);
  l->append(", ");
  LogParam(p.matchCase, l);
  l->append(", ");
  LogParam(p.findNext, l);
  l->append(")");
}

void ParamTraits<WebKit::WebCache::ResourceTypeStat>::Log(
    const param_type& p, std::string* l) {
  l->append(base::StringPrintf("%" PRIuS " %" PRIuS " %" PRIuS " %" PRIuS,
                               p.count, p.size, p.liveSize, p.decodedSize));
}

void ParamTraits<WebKit::WebMediaPlayerAction>::Write(Message* m,
                                                      const param_type& p) {
  WriteParam(m, static_cast<int>(p.type));
  WriteParam(m, p.enable);
}

bool ParamTraits<WebKit::WebMediaPlayerAction>::Read(const Message* m,
                                                     void** iter,
                                                     param_type* r) {
  int temp;
  if (!ReadParam(m, iter, &temp))
    return false;
  r->type = static_cast<param_type::Type>(temp);
  return ReadParam(m, iter, &r->enable);
}

void ParamTraits<WebKit::WebMediaPlayerAction>::Log(const param_type& p,
                                                    std::string* l) {
  l->append("(");
  switch (p.type) {
    case WebKit::WebMediaPlayerAction::Play:
      l->append("Play");
      break;
    case WebKit::WebMediaPlayerAction::Mute:
      l->append("Mute");
      break;
    case WebKit::WebMediaPlayerAction::Loop:
      l->append("Loop");
      break;
    default:
      l->append("Unknown");
      break;
  }
  l->append(", ");
  LogParam(p.enable, l);
  l->append(")");
}

void ParamTraits<WebKit::WebCompositionUnderline>::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.startOffset);
  WriteParam(m, p.endOffset);
  WriteParam(m, p.color);
  WriteParam(m, p.thick);
}

bool ParamTraits<WebKit::WebCompositionUnderline>::Read(
    const Message* m, void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->startOffset) &&
      ReadParam(m, iter, &p->endOffset) &&
      ReadParam(m, iter, &p->color) &&
      ReadParam(m, iter, &p->thick);
}

void ParamTraits<WebKit::WebCompositionUnderline>::Log(const param_type& p,
                                                       std::string* l) {
  l->append("(");
  LogParam(p.startOffset, l);
  l->append(",");
  LogParam(p.endOffset, l);
  l->append(":");
  LogParam(p.color, l);
  l->append(":");
  LogParam(p.thick, l);
  l->append(")");
}

void ParamTraits<WebKit::WebTextCheckingResult>::Write(Message* m,
                                                       const param_type& p) {
  WriteParam(m, static_cast<int>(p.error()));
  WriteParam(m, p.position());
  WriteParam(m, p.length());
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
  LogParam(static_cast<int>(p.error()), l);
  l->append(", ");
  LogParam(p.position(), l);
  l->append(", ");
  LogParam(p.length(), l);
  l->append(")");
}

}  // namespace IPC
