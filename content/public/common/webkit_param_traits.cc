// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: New trait definitions that will be used by Chrome Frame must be placed
// in common_param_traits2.cc.

#include "content/public/common/webkit_param_traits.h"

#include "base/string_number_conversions.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/content_constants.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/glue/password_form.h"
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

void ParamTraits<webkit_glue::ResourceLoadTimingInfo>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.base_time.is_null());
  if (p.base_time.is_null())
    return;
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
    const Message* m, void** iter, param_type* r) {
  bool is_null;
  if (!ReadParam(m, iter, &is_null))
    return false;
  if (is_null)
    return true;

  return
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
    const Message* m, void** iter, param_type* r) {
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
                                        void** iter,
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
                                           void** iter,
                                           param_type* r) {
  return webkit_glue::DeserializeNPIdentifier(*m, iter, &r->identifier);
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
                                                  void** iter,
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
                                              void** iter,
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

void ParamTraits<webkit_glue::PasswordForm>::Write(Message* m,
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

bool ParamTraits<webkit_glue::PasswordForm>::Read(const Message* m, void** iter,
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
void ParamTraits<webkit_glue::PasswordForm>::Log(const param_type& p,
                                                 std::string* l) {
  l->append("<PasswordForm>");
}

}  // namespace IPC
