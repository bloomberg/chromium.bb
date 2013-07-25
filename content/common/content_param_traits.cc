// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_param_traits.h"

#include "base/strings/string_number_conversions.h"
#include "net/base/ip_endpoint.h"
#include "ui/base/range/range.h"

namespace {

int WebInputEventSizeForType(WebKit::WebInputEvent::Type type) {
  if (WebKit::WebInputEvent::isMouseEventType(type))
    return sizeof(WebKit::WebMouseEvent);
  if (type == WebKit::WebInputEvent::MouseWheel)
    return sizeof(WebKit::WebMouseWheelEvent);
  if (WebKit::WebInputEvent::isKeyboardEventType(type))
    return sizeof(WebKit::WebKeyboardEvent);
  if (WebKit::WebInputEvent::isTouchEventType(type))
    return sizeof(WebKit::WebTouchEvent);
  if (WebKit::WebInputEvent::isGestureEventType(type))
    return sizeof(WebKit::WebGestureEvent);
  NOTREACHED() << "Unknown webkit event type " << type;
  return 0;
}

}  // namespace

namespace IPC {

void ParamTraits<net::IPEndPoint>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.address());
  WriteParam(m, p.port());
}

bool ParamTraits<net::IPEndPoint>::Read(const Message* m, PickleIterator* iter,
                                        param_type* p) {
  net::IPAddressNumber address;
  int port;
  if (!ReadParam(m, iter, &address) || !ReadParam(m, iter, &port))
    return false;
  *p = net::IPEndPoint(address, port);
  return true;
}

void ParamTraits<net::IPEndPoint>::Log(const param_type& p, std::string* l) {
  LogParam("IPEndPoint:" + p.ToString(), l);
}

void ParamTraits<ui::Range>::Write(Message* m, const ui::Range& r) {
  m->WriteUInt64(r.start());
  m->WriteUInt64(r.end());
}

bool ParamTraits<ui::Range>::Read(const Message* m,
                                  PickleIterator* iter,
                                  ui::Range* r) {
  uint64 start, end;
  if (!m->ReadUInt64(iter, &start) || !m->ReadUInt64(iter, &end))
    return false;
  r->set_start(start);
  r->set_end(end);
  return true;
}

void ParamTraits<ui::Range>::Log(const ui::Range& r, std::string* l) {
  l->append(base::StringPrintf("(%" PRIuS ", %" PRIuS ")", r.start(), r.end()));
}

void ParamTraits<WebInputEventPointer>::Write(Message* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(p), p->size);
}

bool ParamTraits<WebInputEventPointer>::Read(const Message* m,
                                             PickleIterator* iter,
                                             param_type* r) {
  const char* data;
  int data_length;
  if (!m->ReadData(iter, &data, &data_length)) {
    NOTREACHED();
    return false;
  }
  if (data_length < static_cast<int>(sizeof(WebKit::WebInputEvent))) {
    NOTREACHED();
    return false;
  }
  param_type event = reinterpret_cast<param_type>(data);
  // Check that the data size matches that of the event.
  if (data_length != static_cast<int>(event->size)) {
    NOTREACHED();
    return false;
  }
  if (data_length != WebInputEventSizeForType(event->type)) {
    NOTREACHED();
    return false;
  }
  *r = event;
  return true;
}

void ParamTraits<WebInputEventPointer>::Log(const param_type& p,
                                            std::string* l) {
  l->append("(");
  LogParam(p->size, l);
  l->append(", ");
  LogParam(p->type, l);
  l->append(", ");
  LogParam(p->timeStampSeconds, l);
  l->append(")");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC
