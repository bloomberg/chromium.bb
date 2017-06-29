// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resource_messages.h"

#include "net/base/load_timing_info.h"
#include "net/http/http_response_headers.h"

namespace IPC {

void ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::GetSize(
    base::PickleSizer* s, const param_type& p) {
  GetParamSize(s, p.get() != NULL);
  if (p.get()) {
    base::Pickle temp;
    p->Persist(&temp, net::HttpResponseHeaders::PERSIST_SANS_COOKIES);
    s->AddBytes(temp.payload_size());
  }
}

void ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p.get()) {
    // Do not disclose Set-Cookie headers over IPC.
    p->Persist(m, net::HttpResponseHeaders::PERSIST_SANS_COOKIES);
  }
}

bool ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (has_object)
    *r = new net::HttpResponseHeaders(iter);
  return true;
}

void ParamTraits<scoped_refptr<net::HttpResponseHeaders> >::Log(
    const param_type& p, std::string* l) {
  l->append("<HttpResponseHeaders>");
}

namespace {

void GetCertSize(base::PickleSizer* s, net::X509Certificate* cert) {
  GetParamSize(s, !!cert);
  if (cert) {
    base::Pickle temp;
    cert->Persist(&temp);
    s->AddBytes(temp.payload_size());
  }
}

void WriteCert(base::Pickle* m, net::X509Certificate* cert) {
  WriteParam(m, !!cert);
  if (cert)
    cert->Persist(m);
}

bool ReadCert(const base::Pickle* m,
              base::PickleIterator* iter,
              scoped_refptr<net::X509Certificate>* cert) {
  DCHECK(!*cert);
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  *cert = net::X509Certificate::CreateFromPickle(
      iter, net::X509Certificate::PICKLETYPE_CERTIFICATE_CHAIN_V3);
  return !!cert->get();
}

}  // namespace

void ParamTraits<net::SSLInfo>::GetSize(base::PickleSizer* s,
                                        const param_type& p) {
  GetParamSize(s, p.is_valid());
  if (!p.is_valid())
    return;
  GetCertSize(s, p.cert.get());
  GetCertSize(s, p.unverified_cert.get());
  GetParamSize(s, p.cert_status);
  GetParamSize(s, p.security_bits);
  GetParamSize(s, p.key_exchange_group);
  GetParamSize(s, p.connection_status);
  GetParamSize(s, p.is_issued_by_known_root);
  GetParamSize(s, p.pkp_bypassed);
  GetParamSize(s, p.client_cert_sent);
  GetParamSize(s, p.channel_id_sent);
  GetParamSize(s, p.token_binding_negotiated);
  GetParamSize(s, p.token_binding_key_param);
  GetParamSize(s, p.handshake_type);
  GetParamSize(s, p.public_key_hashes);
  GetParamSize(s, p.pinning_failure_log);
  GetParamSize(s, p.signed_certificate_timestamps);
  GetParamSize(s, p.ct_compliance_details_available);
  GetParamSize(s, p.ct_cert_policy_compliance);
  GetParamSize(s, p.ocsp_result.response_status);
  GetParamSize(s, p.ocsp_result.revocation_status);
}

void ParamTraits<net::SSLInfo>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.is_valid());
  if (!p.is_valid())
    return;
  WriteCert(m, p.cert.get());
  WriteCert(m, p.unverified_cert.get());
  WriteParam(m, p.cert_status);
  WriteParam(m, p.security_bits);
  WriteParam(m, p.key_exchange_group);
  WriteParam(m, p.connection_status);
  WriteParam(m, p.is_issued_by_known_root);
  WriteParam(m, p.pkp_bypassed);
  WriteParam(m, p.client_cert_sent);
  WriteParam(m, p.channel_id_sent);
  WriteParam(m, p.token_binding_negotiated);
  WriteParam(m, p.token_binding_key_param);
  WriteParam(m, p.handshake_type);
  WriteParam(m, p.public_key_hashes);
  WriteParam(m, p.pinning_failure_log);
  WriteParam(m, p.signed_certificate_timestamps);
  WriteParam(m, p.ct_compliance_details_available);
  WriteParam(m, p.ct_cert_policy_compliance);
  WriteParam(m, p.ocsp_result.response_status);
  WriteParam(m, p.ocsp_result.revocation_status);
}

bool ParamTraits<net::SSLInfo>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* r) {
  bool is_valid = false;
  if (!ReadParam(m, iter, &is_valid))
    return false;
  if (!is_valid)
    return true;
  return ReadCert(m, iter, &r->cert) &&
         ReadCert(m, iter, &r->unverified_cert) &&
         ReadParam(m, iter, &r->cert_status) &&
         ReadParam(m, iter, &r->security_bits) &&
         ReadParam(m, iter, &r->key_exchange_group) &&
         ReadParam(m, iter, &r->connection_status) &&
         ReadParam(m, iter, &r->is_issued_by_known_root) &&
         ReadParam(m, iter, &r->pkp_bypassed) &&
         ReadParam(m, iter, &r->client_cert_sent) &&
         ReadParam(m, iter, &r->channel_id_sent) &&
         ReadParam(m, iter, &r->token_binding_negotiated) &&
         ReadParam(m, iter, &r->token_binding_key_param) &&
         ReadParam(m, iter, &r->handshake_type) &&
         ReadParam(m, iter, &r->public_key_hashes) &&
         ReadParam(m, iter, &r->pinning_failure_log) &&
         ReadParam(m, iter, &r->signed_certificate_timestamps) &&
         ReadParam(m, iter, &r->ct_compliance_details_available) &&
         ReadParam(m, iter, &r->ct_cert_policy_compliance) &&
         ReadParam(m, iter, &r->ocsp_result.response_status) &&
         ReadParam(m, iter, &r->ocsp_result.revocation_status);
}

void ParamTraits<net::SSLInfo>::Log(const param_type& p, std::string* l) {
  l->append("<SSLInfo>");
}

void ParamTraits<net::HashValue>::GetSize(base::PickleSizer* s,
                                          const param_type& p) {
  GetParamSize(s, p.ToString());
}

void ParamTraits<net::HashValue>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.ToString());
}

bool ParamTraits<net::HashValue>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  std::string str;
  return ReadParam(m, iter, &str) && r->FromString(str);
}

void ParamTraits<net::HashValue>::Log(const param_type& p, std::string* l) {
  l->append("<HashValue>");
}

void ParamTraits<storage::DataElement>::GetSize(base::PickleSizer* s,
                                                const param_type& p) {
  GetParamSize(s, static_cast<int>(p.type()));
  switch (p.type()) {
    case storage::DataElement::TYPE_BYTES: {
      s->AddData(static_cast<int>(p.length()));
      break;
    }
    case storage::DataElement::TYPE_BYTES_DESCRIPTION: {
      GetParamSize(s, p.length());
      break;
    }
    case storage::DataElement::TYPE_FILE: {
      GetParamSize(s, p.path());
      GetParamSize(s, p.offset());
      GetParamSize(s, p.length());
      GetParamSize(s, p.expected_modification_time());
      break;
    }
    case storage::DataElement::TYPE_FILE_FILESYSTEM: {
      GetParamSize(s, p.filesystem_url());
      GetParamSize(s, p.offset());
      GetParamSize(s, p.length());
      GetParamSize(s, p.expected_modification_time());
      break;
    }
    case storage::DataElement::TYPE_BLOB: {
      GetParamSize(s, p.blob_uuid());
      GetParamSize(s, p.offset());
      GetParamSize(s, p.length());
      break;
    }
    case storage::DataElement::TYPE_DISK_CACHE_ENTRY: {
      NOTREACHED() << "Can't be sent by IPC.";
      break;
    }
    case storage::DataElement::TYPE_UNKNOWN: {
      NOTREACHED();
      break;
    }
  }
}

void ParamTraits<storage::DataElement>::Write(base::Pickle* m,
                                              const param_type& p) {
  WriteParam(m, static_cast<int>(p.type()));
  switch (p.type()) {
    case storage::DataElement::TYPE_BYTES: {
      m->WriteData(p.bytes(), static_cast<int>(p.length()));
      break;
    }
    case storage::DataElement::TYPE_BYTES_DESCRIPTION: {
      WriteParam(m, p.length());
      break;
    }
    case storage::DataElement::TYPE_FILE: {
      WriteParam(m, p.path());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      WriteParam(m, p.expected_modification_time());
      break;
    }
    case storage::DataElement::TYPE_FILE_FILESYSTEM: {
      WriteParam(m, p.filesystem_url());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      WriteParam(m, p.expected_modification_time());
      break;
    }
    case storage::DataElement::TYPE_BLOB: {
      WriteParam(m, p.blob_uuid());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      break;
    }
    case storage::DataElement::TYPE_DISK_CACHE_ENTRY: {
      NOTREACHED() << "Can't be sent by IPC.";
      break;
    }
    case storage::DataElement::TYPE_UNKNOWN: {
      NOTREACHED();
      break;
    }
  }
}

bool ParamTraits<storage::DataElement>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;
  switch (type) {
    case storage::DataElement::TYPE_BYTES: {
      const char* data;
      int len;
      if (!iter->ReadData(&data, &len))
        return false;
      r->SetToBytes(data, len);
      return true;
    }
    case storage::DataElement::TYPE_BYTES_DESCRIPTION: {
      uint64_t length;
      if (!ReadParam(m, iter, &length))
        return false;
      r->SetToBytesDescription(length);
      return true;
    }
    case storage::DataElement::TYPE_FILE: {
      base::FilePath file_path;
      uint64_t offset, length;
      base::Time expected_modification_time;
      if (!ReadParam(m, iter, &file_path))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      if (!ReadParam(m, iter, &expected_modification_time))
        return false;
      r->SetToFilePathRange(file_path, offset, length,
                            expected_modification_time);
      return true;
    }
    case storage::DataElement::TYPE_FILE_FILESYSTEM: {
      GURL file_system_url;
      uint64_t offset, length;
      base::Time expected_modification_time;
      if (!ReadParam(m, iter, &file_system_url))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      if (!ReadParam(m, iter, &expected_modification_time))
        return false;
      r->SetToFileSystemUrlRange(file_system_url, offset, length,
                                 expected_modification_time);
      return true;
    }
    case storage::DataElement::TYPE_BLOB: {
      std::string blob_uuid;
      uint64_t offset, length;
      if (!ReadParam(m, iter, &blob_uuid))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      r->SetToBlobRange(blob_uuid, offset, length);
      return true;
    }
    case storage::DataElement::TYPE_DISK_CACHE_ENTRY: {
      NOTREACHED() << "Can't be sent by IPC.";
      return false;
    }
    case storage::DataElement::TYPE_UNKNOWN: {
      NOTREACHED();
      return false;
    }
  }
  return false;
}

void ParamTraits<storage::DataElement>::Log(const param_type& p,
                                            std::string* l) {
  l->append("<storage::DataElement>");
}

void ParamTraits<scoped_refptr<content::ResourceDevToolsInfo>>::GetSize(
    base::PickleSizer* s, const param_type& p) {
  GetParamSize(s, p.get() != NULL);
  if (p.get()) {
    GetParamSize(s, p->http_status_code);
    GetParamSize(s, p->http_status_text);
    GetParamSize(s, p->request_headers);
    GetParamSize(s, p->response_headers);
    GetParamSize(s, p->request_headers_text);
    GetParamSize(s, p->response_headers_text);
  }
}

void ParamTraits<scoped_refptr<content::ResourceDevToolsInfo>>::Write(
    base::Pickle* m,
    const param_type& p) {
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

bool ParamTraits<scoped_refptr<content::ResourceDevToolsInfo>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  *r = new content::ResourceDevToolsInfo();
  return
      ReadParam(m, iter, &(*r)->http_status_code) &&
      ReadParam(m, iter, &(*r)->http_status_text) &&
      ReadParam(m, iter, &(*r)->request_headers) &&
      ReadParam(m, iter, &(*r)->response_headers) &&
      ReadParam(m, iter, &(*r)->request_headers_text) &&
      ReadParam(m, iter, &(*r)->response_headers_text);
}

void ParamTraits<scoped_refptr<content::ResourceDevToolsInfo> >::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  if (p.get()) {
    LogParam(p->request_headers, l);
    l->append(", ");
    LogParam(p->response_headers, l);
  }
  l->append(")");
}

void ParamTraits<net::LoadTimingInfo>::GetSize(base::PickleSizer* s,
                                               const param_type& p) {
  GetParamSize(s, p.socket_log_id);
  GetParamSize(s, p.socket_reused);
  GetParamSize(s, p.request_start_time.is_null());
  if (p.request_start_time.is_null())
    return;
  GetParamSize(s, p.request_start_time);
  GetParamSize(s, p.request_start);
  GetParamSize(s, p.proxy_resolve_start);
  GetParamSize(s, p.proxy_resolve_end);
  GetParamSize(s, p.connect_timing.dns_start);
  GetParamSize(s, p.connect_timing.dns_end);
  GetParamSize(s, p.connect_timing.connect_start);
  GetParamSize(s, p.connect_timing.connect_end);
  GetParamSize(s, p.connect_timing.ssl_start);
  GetParamSize(s, p.connect_timing.ssl_end);
  GetParamSize(s, p.send_start);
  GetParamSize(s, p.send_end);
  GetParamSize(s, p.receive_headers_end);
  GetParamSize(s, p.push_start);
  GetParamSize(s, p.push_end);
}

void ParamTraits<net::LoadTimingInfo>::Write(base::Pickle* m,
                                             const param_type& p) {
  WriteParam(m, p.socket_log_id);
  WriteParam(m, p.socket_reused);
  WriteParam(m, p.request_start_time.is_null());
  if (p.request_start_time.is_null())
    return;
  WriteParam(m, p.request_start_time);
  WriteParam(m, p.request_start);
  WriteParam(m, p.proxy_resolve_start);
  WriteParam(m, p.proxy_resolve_end);
  WriteParam(m, p.connect_timing.dns_start);
  WriteParam(m, p.connect_timing.dns_end);
  WriteParam(m, p.connect_timing.connect_start);
  WriteParam(m, p.connect_timing.connect_end);
  WriteParam(m, p.connect_timing.ssl_start);
  WriteParam(m, p.connect_timing.ssl_end);
  WriteParam(m, p.send_start);
  WriteParam(m, p.send_end);
  WriteParam(m, p.receive_headers_end);
  WriteParam(m, p.push_start);
  WriteParam(m, p.push_end);
}

bool ParamTraits<net::LoadTimingInfo>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* r) {
  bool has_no_times;
  if (!ReadParam(m, iter, &r->socket_log_id) ||
      !ReadParam(m, iter, &r->socket_reused) ||
      !ReadParam(m, iter, &has_no_times)) {
    return false;
  }
  if (has_no_times)
    return true;

  return
      ReadParam(m, iter, &r->request_start_time) &&
      ReadParam(m, iter, &r->request_start) &&
      ReadParam(m, iter, &r->proxy_resolve_start) &&
      ReadParam(m, iter, &r->proxy_resolve_end) &&
      ReadParam(m, iter, &r->connect_timing.dns_start) &&
      ReadParam(m, iter, &r->connect_timing.dns_end) &&
      ReadParam(m, iter, &r->connect_timing.connect_start) &&
      ReadParam(m, iter, &r->connect_timing.connect_end) &&
      ReadParam(m, iter, &r->connect_timing.ssl_start) &&
      ReadParam(m, iter, &r->connect_timing.ssl_end) &&
      ReadParam(m, iter, &r->send_start) &&
      ReadParam(m, iter, &r->send_end) &&
      ReadParam(m, iter, &r->receive_headers_end) &&
      ReadParam(m, iter, &r->push_start) &&
      ReadParam(m, iter, &r->push_end);
}

void ParamTraits<net::LoadTimingInfo>::Log(const param_type& p,
                                           std::string* l) {
  l->append("(");
  LogParam(p.socket_log_id, l);
  l->append(",");
  LogParam(p.socket_reused, l);
  l->append(",");
  LogParam(p.request_start_time, l);
  l->append(", ");
  LogParam(p.request_start, l);
  l->append(", ");
  LogParam(p.proxy_resolve_start, l);
  l->append(", ");
  LogParam(p.proxy_resolve_end, l);
  l->append(", ");
  LogParam(p.connect_timing.dns_start, l);
  l->append(", ");
  LogParam(p.connect_timing.dns_end, l);
  l->append(", ");
  LogParam(p.connect_timing.connect_start, l);
  l->append(", ");
  LogParam(p.connect_timing.connect_end, l);
  l->append(", ");
  LogParam(p.connect_timing.ssl_start, l);
  l->append(", ");
  LogParam(p.connect_timing.ssl_end, l);
  l->append(", ");
  LogParam(p.send_start, l);
  l->append(", ");
  LogParam(p.send_end, l);
  l->append(", ");
  LogParam(p.receive_headers_end, l);
  l->append(", ");
  LogParam(p.push_start, l);
  l->append(", ");
  LogParam(p.push_end, l);
  l->append(")");
}

void ParamTraits<scoped_refptr<content::ResourceRequestBody>>::GetSize(
    base::PickleSizer* s,
    const param_type& p) {
  GetParamSize(s, p.get() != NULL);
  if (p.get()) {
    GetParamSize(s, *p->elements());
    GetParamSize(s, p->identifier());
    GetParamSize(s, p->contains_sensitive_info());
  }
}

void ParamTraits<scoped_refptr<content::ResourceRequestBody>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p.get()) {
    WriteParam(m, *p->elements());
    WriteParam(m, p->identifier());
    WriteParam(m, p->contains_sensitive_info());
  }
}

bool ParamTraits<scoped_refptr<content::ResourceRequestBody>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  std::vector<storage::DataElement> elements;
  if (!ReadParam(m, iter, &elements))
    return false;
  int64_t identifier;
  if (!ReadParam(m, iter, &identifier))
    return false;
  bool contains_sensitive_info;
  if (!ReadParam(m, iter, &contains_sensitive_info))
    return false;
  *r = new content::ResourceRequestBody;
  (*r)->swap_elements(&elements);
  (*r)->set_identifier(identifier);
  (*r)->set_contains_sensitive_info(contains_sensitive_info);
  return true;
}

void ParamTraits<scoped_refptr<content::ResourceRequestBody>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<ResourceRequestBody>");
}

void ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::GetSize(
    base::PickleSizer* s,
    const param_type& p) {
  GetParamSize(s, p.get() != NULL);
  if (p.get()) {
    GetParamSize(s, static_cast<unsigned int>(p->version));
    GetParamSize(s, p->log_id);
    GetParamSize(s, p->timestamp);
    GetParamSize(s, p->extensions);
    GetParamSize(s, static_cast<unsigned int>(p->signature.hash_algorithm));
    GetParamSize(s,
                 static_cast<unsigned int>(p->signature.signature_algorithm));
    GetParamSize(s, p->signature.signature_data);
    GetParamSize(s, static_cast<unsigned int>(p->origin));
    GetParamSize(s, p->log_description);
  }
}

void ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p.get())
    p->Persist(m);
}

bool ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (has_object)
    *r = net::ct::SignedCertificateTimestamp::CreateFromPickle(iter);
  return true;
}

void ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<SignedCertificateTimestamp>");
}

}  // namespace IPC
