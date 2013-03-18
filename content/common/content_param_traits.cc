// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_param_traits.h"

#include "base/string_number_conversions.h"
#include "net/base/ip_endpoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/glue/npruntime_util.h"
#include "webkit/plugins/npapi/plugin_host.h"
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

namespace content {

NPIdentifier_Param::NPIdentifier_Param()
    : identifier() {
}

NPIdentifier_Param::~NPIdentifier_Param() {
}

NPVariant_Param::NPVariant_Param()
    : type(NPVARIANT_PARAM_VOID),
      bool_value(false),
      int_value(0),
      double_value(0),
      npobject_routing_id(-1) {
}

NPVariant_Param::~NPVariant_Param() {
}

}  // namespace content

using content::NPIdentifier_Param;
using content::NPVariant_Param;

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

void ParamTraits<NPVariant_Param>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.type));
  if (p.type == content::NPVARIANT_PARAM_BOOL) {
    WriteParam(m, p.bool_value);
  } else if (p.type == content::NPVARIANT_PARAM_INT) {
    WriteParam(m, p.int_value);
  } else if (p.type == content::NPVARIANT_PARAM_DOUBLE) {
    WriteParam(m, p.double_value);
  } else if (p.type == content::NPVARIANT_PARAM_STRING) {
    WriteParam(m, p.string_value);
  } else if (p.type == content::NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             p.type == content::NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
    // This is the routing id used to connect NPObjectProxy in the other
    // process with NPObjectStub in this process or to identify the raw
    // npobject pointer to be used in the callee process.
    WriteParam(m, p.npobject_routing_id);
  } else {
    DCHECK(p.type == content::NPVARIANT_PARAM_VOID ||
           p.type == content::NPVARIANT_PARAM_NULL);
  }
}

bool ParamTraits<NPVariant_Param>::Read(const Message* m,
                                        PickleIterator* iter,
                                        param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  bool result = false;
  r->type = static_cast<content::NPVariant_ParamEnum>(type);
  if (r->type == content::NPVARIANT_PARAM_BOOL) {
    result = ReadParam(m, iter, &r->bool_value);
  } else if (r->type == content::NPVARIANT_PARAM_INT) {
    result = ReadParam(m, iter, &r->int_value);
  } else if (r->type == content::NPVARIANT_PARAM_DOUBLE) {
    result = ReadParam(m, iter, &r->double_value);
  } else if (r->type == content::NPVARIANT_PARAM_STRING) {
    result = ReadParam(m, iter, &r->string_value);
  } else if (r->type == content::NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             r->type == content::NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
    result = ReadParam(m, iter, &r->npobject_routing_id);
  } else if ((r->type == content::NPVARIANT_PARAM_VOID) ||
             (r->type == content::NPVARIANT_PARAM_NULL)) {
    result = true;
  } else {
    NOTREACHED();
  }

  return result;
}

void ParamTraits<NPVariant_Param>::Log(const param_type& p, std::string* l) {
  l->append(
      base::StringPrintf("NPVariant_Param(%d, ", static_cast<int>(p.type)));
  if (p.type == content::NPVARIANT_PARAM_BOOL) {
    LogParam(p.bool_value, l);
  } else if (p.type == content::NPVARIANT_PARAM_INT) {
    LogParam(p.int_value, l);
  } else if (p.type == content::NPVARIANT_PARAM_DOUBLE) {
    LogParam(p.double_value, l);
  } else if (p.type == content::NPVARIANT_PARAM_STRING) {
    LogParam(p.string_value, l);
  } else if (p.type == content::NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             p.type == content::NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
    LogParam(p.npobject_routing_id, l);
  } else {
    l->append("<none>");
  }
  l->append(")");
}

void ParamTraits<NPIdentifier_Param>::Write(Message* m, const param_type& p) {
  webkit_glue::SerializeNPIdentifier(p.identifier, m);
}

bool ParamTraits<NPIdentifier_Param>::Read(const Message* m,
                                           PickleIterator* iter,
                                           param_type* r) {
  return webkit_glue::DeserializeNPIdentifier(iter, &r->identifier);
}

void ParamTraits<NPIdentifier_Param>::Log(const param_type& p, std::string* l) {
  if (WebKit::WebBindings::identifierIsString(p.identifier)) {
    NPUTF8* str = WebKit::WebBindings::utf8FromIdentifier(p.identifier);
    l->append(str);
    webkit::npapi::PluginHost::Singleton()->host_functions()->memfree(str);
  } else {
    l->append(base::IntToString(
        WebKit::WebBindings::intFromIdentifier(p.identifier)));
  }
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
  l->append(base::StringPrintf("(%"PRIuS", %"PRIuS")", r.start(), r.end()));
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
