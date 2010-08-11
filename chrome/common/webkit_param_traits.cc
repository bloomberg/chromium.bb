// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webkit_param_traits.h"

#include "third_party/WebKit/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"

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

void ParamTraits<WebKit::WebRect>::Log(const param_type& p, std::wstring* l) {
  l->append(L"(");
  LogParam(p.x, l);
  l->append(L", ");
  LogParam(p.y, l);
  l->append(L", ");
  LogParam(p.width, l);
  l->append(L", ");
  LogParam(p.height, l);
  l->append(L")");
}

void ParamTraits<WebKit::WebScreenInfo>::Write(Message* m, const param_type& p) {
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
                                             std::wstring* l) {
  l->append(L"(");
  LogParam(p.depth, l);
  l->append(L", ");
  LogParam(p.depthPerComponent, l);
  l->append(L", ");
  LogParam(p.isMonochrome, l);
  l->append(L", ");
  LogParam(p.rect, l);
  l->append(L", ");
  LogParam(p.availableRect, l);
  l->append(L")");
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
                                              std::wstring* l) {
  l->append(L"(");
  LogParam(p.forward, l);
  l->append(L", ");
  LogParam(p.matchCase, l);
  l->append(L", ");
  LogParam(p.findNext, l);
  l->append(L")");
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
                                                       std::wstring* l) {
  l->append(L"(");
  LogParam(p.startOffset, l);
  l->append(L",");
  LogParam(p.endOffset, l);
  l->append(L":");
  LogParam(p.color, l);
  l->append(L":");
  LogParam(p.thick, l);
  l->append(L")");
}

}  // namespace IPC
