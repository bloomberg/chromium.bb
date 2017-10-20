// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resource_messages.h"

#include "ipc/ipc_mojo_param_traits.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_response_headers.h"

namespace IPC {

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
    case storage::DataElement::TYPE_DATA_PIPE: {
      blink::mojom::SizeGetterPtr size_getter;
      WriteParam(
          m,
          const_cast<param_type&>(p).ReleaseDataPipe(&size_getter).release());
      WriteParam(m, size_getter.PassInterface().PassHandle().release());
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
    case storage::DataElement::TYPE_DATA_PIPE: {
      mojo::DataPipeConsumerHandle data_pipe;
      blink::mojom::SizeGetterPtr size_getter;
      if (!ReadParam(m, iter, &data_pipe))
        return false;
      mojo::MessagePipeHandle message_pipe;
      if (!ReadParam(m, iter, &message_pipe))
        return false;
      size_getter.Bind(blink::mojom::SizeGetterPtrInfo(
          mojo::ScopedMessagePipeHandle(message_pipe), 0u));
      r->SetToDataPipe(mojo::ScopedDataPipeConsumerHandle(data_pipe),
                       std::move(size_getter));
      return true;
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
