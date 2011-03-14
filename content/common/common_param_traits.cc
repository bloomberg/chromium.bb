// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/common_param_traits.h"

#include "content/common/content_constants.h"
#include "net/base/host_port_pair.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace IPC {

void ParamTraits<GURL>::Write(Message* m, const GURL& p) {
  m->WriteString(p.possibly_invalid_spec());
  // TODO(brettw) bug 684583: Add encoding for query params.
}

bool ParamTraits<GURL>::Read(const Message* m, void** iter, GURL* p) {
  std::string s;
  if (!m->ReadString(iter, &s) || s.length() > content::kMaxURLChars) {
    *p = GURL();
    return false;
  }
  *p = GURL(s);
  return true;
}

void ParamTraits<GURL>::Log(const GURL& p, std::string* l) {
  l->append(p.spec());
}


void ParamTraits<ResourceType::Type>::Write(Message* m, const param_type& p) {
  m->WriteInt(p);
}

bool ParamTraits<ResourceType::Type>::Read(const Message* m,
                                           void** iter,
                                           param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type) || !ResourceType::ValidType(type))
    return false;
  *p = ResourceType::FromInt(type);
  return true;
}

void ParamTraits<ResourceType::Type>::Log(const param_type& p, std::string* l) {
  std::string type;
  switch (p) {
    case ResourceType::MAIN_FRAME:
      type = "MAIN_FRAME";
      break;
    case ResourceType::SUB_FRAME:
      type = "SUB_FRAME";
      break;
    case ResourceType::SUB_RESOURCE:
      type = "SUB_RESOURCE";
      break;
    case ResourceType::OBJECT:
      type = "OBJECT";
      break;
    case ResourceType::MEDIA:
      type = "MEDIA";
      break;
    default:
      type = "UNKNOWN";
      break;
  }

  LogParam(type, l);
}

void ParamTraits<net::URLRequestStatus>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, static_cast<int>(p.status()));
  WriteParam(m, p.os_error());
}

bool ParamTraits<net::URLRequestStatus>::Read(const Message* m, void** iter,
                                              param_type* r) {
  int status, os_error;
  if (!ReadParam(m, iter, &status) ||
      !ReadParam(m, iter, &os_error))
    return false;
  r->set_status(static_cast<net::URLRequestStatus::Status>(status));
  r->set_os_error(os_error);
  return true;
}

void ParamTraits<net::URLRequestStatus>::Log(const param_type& p,
                                             std::string* l) {
  std::string status;
  switch (p.status()) {
    case net::URLRequestStatus::SUCCESS:
      status = "SUCCESS";
      break;
    case net::URLRequestStatus::IO_PENDING:
      status = "IO_PENDING ";
      break;
    case net::URLRequestStatus::HANDLED_EXTERNALLY:
      status = "HANDLED_EXTERNALLY";
      break;
    case net::URLRequestStatus::CANCELED:
      status = "CANCELED";
      break;
    case net::URLRequestStatus::FAILED:
      status = "FAILED";
      break;
    default:
      status = "UNKNOWN";
      break;
  }
  if (p.status() == net::URLRequestStatus::FAILED)
    l->append("(");

  LogParam(status, l);

  if (p.status() == net::URLRequestStatus::FAILED) {
    l->append(", ");
    LogParam(p.os_error(), l);
    l->append(")");
  }
}

// Only the net::UploadData ParamTraits<> definition needs this definition, so
// keep this in the implementation file so we can forward declare UploadData in
// the header.
template <>
struct ParamTraits<net::UploadData::Element> {
  typedef net::UploadData::Element param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type()));
    switch (p.type()) {
      case net::UploadData::TYPE_BYTES: {
        m->WriteData(&p.bytes()[0], static_cast<int>(p.bytes().size()));
        break;
      }
      case net::UploadData::TYPE_CHUNK: {
        std::string chunk_length = StringPrintf(
            "%X\r\n", static_cast<unsigned int>(p.bytes().size()));
        std::vector<char> bytes;
        bytes.insert(bytes.end(), chunk_length.data(),
                     chunk_length.data() + chunk_length.length());
        const char* data = &p.bytes()[0];
        bytes.insert(bytes.end(), data, data + p.bytes().size());
        const char* crlf = "\r\n";
        bytes.insert(bytes.end(), crlf, crlf + strlen(crlf));
        if (p.is_last_chunk()) {
          const char* end_of_data = "0\r\n\r\n";
          bytes.insert(bytes.end(), end_of_data,
                       end_of_data + strlen(end_of_data));
        }
        m->WriteData(&bytes[0], static_cast<int>(bytes.size()));
        // If this element is part of a chunk upload then send over information
        // indicating if this is the last chunk.
        WriteParam(m, p.is_last_chunk());
        break;
      }
      case net::UploadData::TYPE_FILE: {
        WriteParam(m, p.file_path());
        WriteParam(m, p.file_range_offset());
        WriteParam(m, p.file_range_length());
        WriteParam(m, p.expected_file_modification_time());
        break;
      }
      default: {
        WriteParam(m, p.blob_url());
        break;
      }
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;
    switch (type) {
      case net::UploadData::TYPE_BYTES: {
        const char* data;
        int len;
        if (!m->ReadData(iter, &data, &len))
          return false;
        r->SetToBytes(data, len);
        break;
      }
      case net::UploadData::TYPE_CHUNK: {
        const char* data;
        int len;
        if (!m->ReadData(iter, &data, &len))
          return false;
        r->SetToBytes(data, len);
        // If this element is part of a chunk upload then we need to explicitly
        // set the type of the element and whether it is the last chunk.
        bool is_last_chunk = false;
        if (!ReadParam(m, iter, &is_last_chunk))
          return false;
        r->set_type(net::UploadData::TYPE_CHUNK);
        r->set_is_last_chunk(is_last_chunk);
        break;
      }
      case net::UploadData::TYPE_FILE: {
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
        r->SetToFilePathRange(file_path, offset, length,
                              expected_modification_time);
        break;
      }
      default: {
        DCHECK(type == net::UploadData::TYPE_BLOB);
        GURL blob_url;
        if (!ReadParam(m, iter, &blob_url))
          return false;
        r->SetToBlobUrl(blob_url);
        break;
      }
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<net::UploadData::Element>");
  }
};

void ParamTraits<scoped_refptr<net::UploadData> >::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p) {
    WriteParam(m, *p->elements());
    WriteParam(m, p->identifier());
    WriteParam(m, p->is_chunked());
  }
}

bool ParamTraits<scoped_refptr<net::UploadData> >::Read(const Message* m,
                                                        void** iter,
                                                        param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  std::vector<net::UploadData::Element> elements;
  if (!ReadParam(m, iter, &elements))
    return false;
  int64 identifier;
  if (!ReadParam(m, iter, &identifier))
    return false;
  bool is_chunked = false;
  if (!ReadParam(m, iter, &is_chunked))
    return false;
  *r = new net::UploadData;
  (*r)->swap_elements(&elements);
  (*r)->set_identifier(identifier);
  (*r)->set_is_chunked(is_chunked);
  return true;
}

void ParamTraits<scoped_refptr<net::UploadData> >::Log(const param_type& p,
                                                       std::string* l) {
  l->append("<net::UploadData>");
}

void ParamTraits<net::HostPortPair>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.host());
  WriteParam(m, p.port());
}

bool ParamTraits<net::HostPortPair>::Read(const Message* m, void** iter,
                                          param_type* r) {
  std::string host;
  uint16 port;
  if (!ReadParam(m, iter, &host) || !ReadParam(m, iter, &port))
    return false;

  r->set_host(host);
  r->set_port(port);
  return true;
}

void ParamTraits<net::HostPortPair>::Log(const param_type& p, std::string* l) {
  l->append(p.ToString());
}

void ParamTraits<scoped_refptr<net::HttpResponseHeaders> >::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p) {
    // Do not disclose Set-Cookie headers over IPC.
    p->Persist(m, net::HttpResponseHeaders::PERSIST_SANS_COOKIES);
  }
}

bool ParamTraits<scoped_refptr<net::HttpResponseHeaders> >::Read(
    const Message* m, void** iter, param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (has_object)
    *r = new net::HttpResponseHeaders(*m, iter);
  return true;
}

void ParamTraits<scoped_refptr<net::HttpResponseHeaders> >::Log(
    const param_type& p, std::string* l) {
  l->append("<HttpResponseHeaders>");
}

void ParamTraits<net::IPEndPoint>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.address());
  WriteParam(m, p.port());
}

bool ParamTraits<net::IPEndPoint>::Read(const Message* m, void** iter,
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
      ReadParam(m, iter, &(*r)->response_headers);
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

void ParamTraits<base::PlatformFileInfo>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.size);
  WriteParam(m, p.is_directory);
  WriteParam(m, p.last_modified.ToDoubleT());
  WriteParam(m, p.last_accessed.ToDoubleT());
  WriteParam(m, p.creation_time.ToDoubleT());
}

bool ParamTraits<base::PlatformFileInfo>::Read(
    const Message* m, void** iter, param_type* p) {
  double last_modified;
  double last_accessed;
  double creation_time;
  bool result =
      ReadParam(m, iter, &p->size) &&
      ReadParam(m, iter, &p->is_directory) &&
      ReadParam(m, iter, &last_modified) &&
      ReadParam(m, iter, &last_accessed) &&
      ReadParam(m, iter, &creation_time);
  if (result) {
    p->last_modified = base::Time::FromDoubleT(last_modified);
    p->last_accessed = base::Time::FromDoubleT(last_accessed);
    p->creation_time = base::Time::FromDoubleT(creation_time);
  }
  return result;
}

void ParamTraits<base::PlatformFileInfo>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.size, l);
  l->append(",");
  LogParam(p.is_directory, l);
  l->append(",");
  LogParam(p.last_modified.ToDoubleT(), l);
  l->append(",");
  LogParam(p.last_accessed.ToDoubleT(), l);
  l->append(",");
  LogParam(p.creation_time.ToDoubleT(), l);
  l->append(")");
}

void ParamTraits<gfx::Point>::Write(Message* m, const gfx::Point& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
}

bool ParamTraits<gfx::Point>::Read(const Message* m, void** iter,
                                   gfx::Point* r) {
  int x, y;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Point>::Log(const gfx::Point& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", p.x(), p.y()));
}

void ParamTraits<gfx::Size>::Write(Message* m, const gfx::Size& p) {
  m->WriteInt(p.width());
  m->WriteInt(p.height());
}

bool ParamTraits<gfx::Size>::Read(const Message* m, void** iter, gfx::Size* r) {
  int w, h;
  if (!m->ReadInt(iter, &w) ||
      !m->ReadInt(iter, &h))
    return false;
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::Size>::Log(const gfx::Size& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", p.width(), p.height()));
}

void ParamTraits<gfx::Rect>::Write(Message* m, const gfx::Rect& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
  m->WriteInt(p.width());
  m->WriteInt(p.height());
}

bool ParamTraits<gfx::Rect>::Read(const Message* m, void** iter, gfx::Rect* r) {
  int x, y, w, h;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y) ||
      !m->ReadInt(iter, &w) ||
      !m->ReadInt(iter, &h))
    return false;
  r->set_x(x);
  r->set_y(y);
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::Rect>::Log(const gfx::Rect& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d, %d, %d)", p.x(), p.y(),
                               p.width(), p.height()));
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

}  // namespace IPC
