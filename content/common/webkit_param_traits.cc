// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: New trait definitions that will be used by Chrome Frame must be placed
// in common_param_traits2.cc.

#include "content/common/webkit_param_traits.h"

#include "base/string_number_conversions.h"
#include "content/common/common_param_traits.h"
#include "content/common/content_constants.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/resource_loader_bridge.h"

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

// Only the webkit_blob::BlobData ParamTraits<> definition needs this
// definition, so keep this in the implementation file so we can forward declare
// BlobData in the header.
template <>
struct ParamTraits<webkit_blob::BlobData::Item> {
  typedef webkit_blob::BlobData::Item param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type()));
    if (p.type() == webkit_blob::BlobData::TYPE_DATA) {
      WriteParam(m, p.data());
    } else if (p.type() == webkit_blob::BlobData::TYPE_FILE) {
      WriteParam(m, p.file_path());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      WriteParam(m, p.expected_modification_time());
    } else {
      WriteParam(m, p.blob_url());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;
    if (type == webkit_blob::BlobData::TYPE_DATA) {
      std::string data;
      if (!ReadParam(m, iter, &data))
        return false;
      r->SetToData(data);
    } else if (type == webkit_blob::BlobData::TYPE_FILE) {
      FilePath file_path;
      uint64 offset, length;
      base::Time expected_modification_time;
      if (!ReadParam(m, iter, &file_path))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      if (!ReadParam(m, iter, &expected_modification_time))
        return false;
      r->SetToFile(file_path, offset, length, expected_modification_time);
    } else {
      DCHECK(type == webkit_blob::BlobData::TYPE_BLOB);
      GURL blob_url;
      uint64 offset, length;
      if (!ReadParam(m, iter, &blob_url))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      r->SetToBlob(blob_url, offset, length);
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<BlobData::Item>");
  }
};

void ParamTraits<scoped_refptr<webkit_blob::BlobData> >::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p) {
    WriteParam(m, p->items());
    WriteParam(m, p->content_type());
    WriteParam(m, p->content_disposition());
  }
}

bool ParamTraits<scoped_refptr<webkit_blob::BlobData> >::Read(
    const Message* m, void** iter, param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  std::vector<webkit_blob::BlobData::Item> items;
  if (!ReadParam(m, iter, &items))
    return false;
  std::string content_type;
  if (!ReadParam(m, iter, &content_type))
    return false;
  std::string content_disposition;
  if (!ReadParam(m, iter, &content_disposition))
    return false;
  *r = new webkit_blob::BlobData;
  (*r)->swap_items(&items);
  (*r)->set_content_type(content_type);
  (*r)->set_content_disposition(content_disposition);
  return true;
}

void ParamTraits<scoped_refptr<webkit_blob::BlobData> >::Log(
    const param_type& p, std::string* l) {
  l->append("<webkit_blob::BlobData>");
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
    NPN_MemFree(str);
  } else {
    l->append(base::IntToString(
        WebKit::WebBindings::intFromIdentifier(p.identifier)));
  }
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
