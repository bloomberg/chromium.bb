// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: New trait definitions that will be used by Chrome Frame must be placed
// in common_param_traits2.cc.

#include "content/public/common/webkit_param_traits.h"

#include "base/string_number_conversions.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/content_constants.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/forms/password_form.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/plugins/npapi/plugin_host.h"

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

namespace IPC {

void ParamTraits<WebKit::WebData>::Write(Message* m, const param_type& p) {
  if (p.isEmpty()) {
    m->WriteData(NULL, 0);
  } else {
    m->WriteData(p.data(), p.size());
  }
}

bool ParamTraits<WebKit::WebData>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  const char *data = NULL;
  int data_size = 0;
  if (!m->ReadData(iter, &data, &data_size) || data_size < 0)
    return false;
  if (data_size)
    r->assign(data, data_size);
  else
    r->reset();
  return true;
}

void ParamTraits<WebKit::WebData>::Log(const param_type& p, std::string* l) {
  l->append("(WebData of size ");
  LogParam(p.size(), l);
  l->append(")");
}

void ParamTraits<WebKit::WebTransformationMatrix>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.m11());
  WriteParam(m, p.m12());
  WriteParam(m, p.m13());
  WriteParam(m, p.m14());
  WriteParam(m, p.m21());
  WriteParam(m, p.m22());
  WriteParam(m, p.m23());
  WriteParam(m, p.m24());
  WriteParam(m, p.m31());
  WriteParam(m, p.m32());
  WriteParam(m, p.m33());
  WriteParam(m, p.m34());
  WriteParam(m, p.m41());
  WriteParam(m, p.m42());
  WriteParam(m, p.m43());
  WriteParam(m, p.m44());
}

bool ParamTraits<WebKit::WebTransformationMatrix>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  double m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34,
         m41, m42, m43, m44;
  bool success =
      ReadParam(m, iter, &m11) &&
      ReadParam(m, iter, &m12) &&
      ReadParam(m, iter, &m13) &&
      ReadParam(m, iter, &m14) &&
      ReadParam(m, iter, &m21) &&
      ReadParam(m, iter, &m22) &&
      ReadParam(m, iter, &m23) &&
      ReadParam(m, iter, &m24) &&
      ReadParam(m, iter, &m31) &&
      ReadParam(m, iter, &m32) &&
      ReadParam(m, iter, &m33) &&
      ReadParam(m, iter, &m34) &&
      ReadParam(m, iter, &m41) &&
      ReadParam(m, iter, &m42) &&
      ReadParam(m, iter, &m43) &&
      ReadParam(m, iter, &m44);

  if (success) {
    r->setM11(m11);
    r->setM12(m12);
    r->setM13(m13);
    r->setM14(m14);
    r->setM21(m21);
    r->setM22(m22);
    r->setM23(m23);
    r->setM24(m24);
    r->setM31(m31);
    r->setM32(m32);
    r->setM33(m33);
    r->setM34(m34);
    r->setM41(m41);
    r->setM42(m42);
    r->setM43(m43);
    r->setM44(m44);
  }

  return success;
}

void ParamTraits<WebKit::WebTransformationMatrix>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.m11(), l);
  l->append(", ");
  LogParam(p.m12(), l);
  l->append(", ");
  LogParam(p.m13(), l);
  l->append(", ");
  LogParam(p.m14(), l);
  l->append(", ");
  LogParam(p.m21(), l);
  l->append(", ");
  LogParam(p.m22(), l);
  l->append(", ");
  LogParam(p.m23(), l);
  l->append(", ");
  LogParam(p.m24(), l);
  l->append(", ");
  LogParam(p.m31(), l);
  l->append(", ");
  LogParam(p.m32(), l);
  l->append(", ");
  LogParam(p.m33(), l);
  l->append(", ");
  LogParam(p.m34(), l);
  l->append(", ");
  LogParam(p.m41(), l);
  l->append(", ");
  LogParam(p.m42(), l);
  l->append(", ");
  LogParam(p.m43(), l);
  l->append(", ");
  LogParam(p.m44(), l);
  l->append(") ");
}

void ParamTraits<webkit_glue::ResourceLoadTimingInfo>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.base_time.is_null());
  if (p.base_time.is_null())
    return;
  WriteParam(m, p.base_ticks);
  WriteParam(m, p.base_time);
  WriteParam(m, p.proxy_start);
  WriteParam(m, p.proxy_end);
  WriteParam(m, p.dns_start);
  WriteParam(m, p.dns_end);
  WriteParam(m, p.connect_start);
  WriteParam(m, p.connect_end);
  WriteParam(m, p.ssl_start);
  WriteParam(m, p.ssl_end);
  WriteParam(m, p.send_start);
  WriteParam(m, p.send_end);
  WriteParam(m, p.receive_headers_start);
  WriteParam(m, p.receive_headers_end);
}

bool ParamTraits<webkit_glue::ResourceLoadTimingInfo>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  bool is_null;
  if (!ReadParam(m, iter, &is_null))
    return false;
  if (is_null)
    return true;

  return
      ReadParam(m, iter, &r->base_ticks) &&
      ReadParam(m, iter, &r->base_time) &&
      ReadParam(m, iter, &r->proxy_start) &&
      ReadParam(m, iter, &r->proxy_end) &&
      ReadParam(m, iter, &r->dns_start) &&
      ReadParam(m, iter, &r->dns_end) &&
      ReadParam(m, iter, &r->connect_start) &&
      ReadParam(m, iter, &r->connect_end) &&
      ReadParam(m, iter, &r->ssl_start) &&
      ReadParam(m, iter, &r->ssl_end) &&
      ReadParam(m, iter, &r->send_start) &&
      ReadParam(m, iter, &r->send_end) &&
      ReadParam(m, iter, &r->receive_headers_start) &&
      ReadParam(m, iter, &r->receive_headers_end);
}

void ParamTraits<webkit_glue::ResourceLoadTimingInfo>::Log(const param_type& p,
                                                           std::string* l) {
  l->append("(");
  LogParam(p.base_ticks, l);
  l->append(", ");
  LogParam(p.base_time, l);
  l->append(", ");
  LogParam(p.proxy_start, l);
  l->append(", ");
  LogParam(p.proxy_end, l);
  l->append(", ");
  LogParam(p.dns_start, l);
  l->append(", ");
  LogParam(p.dns_end, l);
  l->append(", ");
  LogParam(p.connect_start, l);
  l->append(", ");
  LogParam(p.connect_end, l);
  l->append(", ");
  LogParam(p.ssl_start, l);
  l->append(", ");
  LogParam(p.ssl_end, l);
  l->append(", ");
  LogParam(p.send_start, l);
  l->append(", ");
  LogParam(p.send_end, l);
  l->append(", ");
  LogParam(p.receive_headers_start, l);
  l->append(", ");
  LogParam(p.receive_headers_end, l);
  l->append(")");
}

void ParamTraits<scoped_refptr<webkit_glue::ResourceDevToolsInfo> >::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p.get()) {
    WriteParam(m, p->http_status_code);
    WriteParam(m, p->http_status_text);
    WriteParam(m, p->request_headers);
    WriteParam(m, p->response_headers);
    WriteParam(m, p->request_headers_text);
    WriteParam(m, p->response_headers_text);
  }
}

bool ParamTraits<scoped_refptr<webkit_glue::ResourceDevToolsInfo> >::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  *r = new webkit_glue::ResourceDevToolsInfo();
  return
      ReadParam(m, iter, &(*r)->http_status_code) &&
      ReadParam(m, iter, &(*r)->http_status_text) &&
      ReadParam(m, iter, &(*r)->request_headers) &&
      ReadParam(m, iter, &(*r)->response_headers) &&
      ReadParam(m, iter, &(*r)->request_headers_text) &&
      ReadParam(m, iter, &(*r)->response_headers_text);
}

void ParamTraits<scoped_refptr<webkit_glue::ResourceDevToolsInfo> >::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  if (p) {
    LogParam(p->request_headers, l);
    l->append(", ");
    LogParam(p->response_headers, l);
  }
  l->append(")");
}

void ParamTraits<NPVariant_Param>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.type));
  if (p.type == NPVARIANT_PARAM_BOOL) {
    WriteParam(m, p.bool_value);
  } else if (p.type == NPVARIANT_PARAM_INT) {
    WriteParam(m, p.int_value);
  } else if (p.type == NPVARIANT_PARAM_DOUBLE) {
    WriteParam(m, p.double_value);
  } else if (p.type == NPVARIANT_PARAM_STRING) {
    WriteParam(m, p.string_value);
  } else if (p.type == NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             p.type == NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
    // This is the routing id used to connect NPObjectProxy in the other
    // process with NPObjectStub in this process or to identify the raw
    // npobject pointer to be used in the callee process.
    WriteParam(m, p.npobject_routing_id);
  } else {
    DCHECK(p.type == NPVARIANT_PARAM_VOID || p.type == NPVARIANT_PARAM_NULL);
  }
}

bool ParamTraits<NPVariant_Param>::Read(const Message* m,
                                        PickleIterator* iter,
                                        param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  bool result = false;
  r->type = static_cast<NPVariant_ParamEnum>(type);
  if (r->type == NPVARIANT_PARAM_BOOL) {
    result = ReadParam(m, iter, &r->bool_value);
  } else if (r->type == NPVARIANT_PARAM_INT) {
    result = ReadParam(m, iter, &r->int_value);
  } else if (r->type == NPVARIANT_PARAM_DOUBLE) {
    result = ReadParam(m, iter, &r->double_value);
  } else if (r->type == NPVARIANT_PARAM_STRING) {
    result = ReadParam(m, iter, &r->string_value);
  } else if (r->type == NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             r->type == NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
    result = ReadParam(m, iter, &r->npobject_routing_id);
  } else if ((r->type == NPVARIANT_PARAM_VOID) ||
             (r->type == NPVARIANT_PARAM_NULL)) {
    result = true;
  } else {
    NOTREACHED();
  }

  return result;
}

void ParamTraits<NPVariant_Param>::Log(const param_type& p, std::string* l) {
  l->append(StringPrintf("NPVariant_Param(%d, ", static_cast<int>(p.type)));
  if (p.type == NPVARIANT_PARAM_BOOL) {
    LogParam(p.bool_value, l);
  } else if (p.type == NPVARIANT_PARAM_INT) {
    LogParam(p.int_value, l);
  } else if (p.type == NPVARIANT_PARAM_DOUBLE) {
    LogParam(p.double_value, l);
  } else if (p.type == NPVARIANT_PARAM_STRING) {
    LogParam(p.string_value, l);
  } else if (p.type == NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             p.type == NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
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

void ParamTraits<webkit::WebPluginMimeType>::Write(Message* m,
                                                   const param_type& p) {
  WriteParam(m, p.mime_type);
  WriteParam(m, p.file_extensions);
  WriteParam(m, p.description);
  WriteParam(m, p.additional_param_names);
  WriteParam(m, p.additional_param_values);
}

bool ParamTraits<webkit::WebPluginMimeType>::Read(const Message* m,
                                                  PickleIterator* iter,
                                                  param_type* p) {
  return
      ReadParam(m, iter, &p->mime_type) &&
      ReadParam(m, iter, &p->file_extensions) &&
      ReadParam(m, iter, &p->description) &&
      ReadParam(m, iter, &p->additional_param_names) &&
      ReadParam(m, iter, &p->additional_param_values);
}

void ParamTraits<webkit::WebPluginMimeType>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.mime_type, l);               l->append(", ");
  LogParam(p.file_extensions, l);         l->append(", ");
  LogParam(p.description, l);             l->append(", ");
  LogParam(p.additional_param_names, l);  l->append(", ");
  LogParam(p.additional_param_values, l);
  l->append(")");
}

void ParamTraits<webkit::WebPluginInfo>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.path);
  WriteParam(m, p.version);
  WriteParam(m, p.desc);
  WriteParam(m, p.mime_types);
  WriteParam(m, p.type);
}

bool ParamTraits<webkit::WebPluginInfo>::Read(const Message* m,
                                              PickleIterator* iter,
                                              param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->path) &&
      ReadParam(m, iter, &p->version) &&
      ReadParam(m, iter, &p->desc) &&
      ReadParam(m, iter, &p->mime_types) &&
      ReadParam(m, iter, &p->type);
}
void ParamTraits<webkit::WebPluginInfo>::Log(const param_type& p,
                                                 std::string* l) {
  l->append("(");
  LogParam(p.name, l);       l->append(", ");
  LogParam(p.path, l);       l->append(", ");
  LogParam(p.version, l);    l->append(", ");
  LogParam(p.desc, l);       l->append(", ");
  LogParam(p.mime_types, l); l->append(", ");
  LogParam(p.type, l);
  l->append(")");
}

void ParamTraits<webkit::forms::PasswordForm>::Write(Message* m,
                                                     const param_type& p) {
  WriteParam(m, p.signon_realm);
  WriteParam(m, p.origin);
  WriteParam(m, p.action);
  WriteParam(m, p.submit_element);
  WriteParam(m, p.username_element);
  WriteParam(m, p.username_value);
  WriteParam(m, p.password_element);
  WriteParam(m, p.password_value);
  WriteParam(m, p.old_password_element);
  WriteParam(m, p.old_password_value);
  WriteParam(m, p.ssl_valid);
  WriteParam(m, p.preferred);
  WriteParam(m, p.blacklisted_by_user);
}

bool ParamTraits<webkit::forms::PasswordForm>::Read(const Message* m,
                                                    PickleIterator* iter,
                                                    param_type* p) {
  return
      ReadParam(m, iter, &p->signon_realm) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->submit_element) &&
      ReadParam(m, iter, &p->username_element) &&
      ReadParam(m, iter, &p->username_value) &&
      ReadParam(m, iter, &p->password_element) &&
      ReadParam(m, iter, &p->password_value) &&
      ReadParam(m, iter, &p->old_password_element) &&
      ReadParam(m, iter, &p->old_password_value) &&
      ReadParam(m, iter, &p->ssl_valid) &&
      ReadParam(m, iter, &p->preferred) &&
      ReadParam(m, iter, &p->blacklisted_by_user);
}
void ParamTraits<webkit::forms::PasswordForm>::Log(const param_type& p,
                                                   std::string* l) {
  l->append("<PasswordForm>");
}

}  // namespace IPC
