// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include "content/public/common/content_constants.h"
#include "content/public/common/referrer.h"
#include "net/base/host_port_pair.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "webkit/glue/resource_request_body.h"

namespace {

struct SkBitmap_Data {
  // The configuration for the bitmap (bits per pixel, etc).
  SkBitmap::Config fConfig;

  // The width of the bitmap in pixels.
  uint32 fWidth;

  // The height of the bitmap in pixels.
  uint32 fHeight;

  void InitSkBitmapDataForTransfer(const SkBitmap& bitmap) {
    fConfig = bitmap.config();
    fWidth = bitmap.width();
    fHeight = bitmap.height();
  }

  // Returns whether |bitmap| successfully initialized.
  bool InitSkBitmapFromData(SkBitmap* bitmap, const char* pixels,
                            size_t total_pixels) const {
    if (total_pixels) {
      bitmap->setConfig(fConfig, fWidth, fHeight, 0);
      if (!bitmap->allocPixels())
        return false;
      if (total_pixels != bitmap->getSize())
        return false;
      memcpy(bitmap->getPixels(), pixels, total_pixels);
    }
    return true;
  }
};

}  // namespace

namespace IPC {

void ParamTraits<GURL>::Write(Message* m, const GURL& p) {
  DCHECK(p.possibly_invalid_spec().length() <= content::kMaxURLChars);
  m->WriteString(p.possibly_invalid_spec());
  // TODO(brettw) bug 684583: Add encoding for query params.
}

bool ParamTraits<GURL>::Read(const Message* m, PickleIterator* iter, GURL* p) {
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

void ParamTraits<net::URLRequestStatus>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, static_cast<int>(p.status()));
  WriteParam(m, p.error());
}

bool ParamTraits<net::URLRequestStatus>::Read(const Message* m,
                                              PickleIterator* iter,
                                              param_type* r) {
  int status, error;
  if (!ReadParam(m, iter, &status) || !ReadParam(m, iter, &error))
    return false;
  r->set_status(static_cast<net::URLRequestStatus::Status>(status));
  r->set_error(error);
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
    LogParam(p.error(), l);
    l->append(")");
  }
}

// Only the net::UploadData ParamTraits<> definition needs this definition, so
// keep this in the implementation file so we can forward declare UploadData in
// the header.
template <>
struct ParamTraits<net::UploadElement> {
  typedef net::UploadElement param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type()));
    switch (p.type()) {
      case net::UploadElement::TYPE_BYTES: {
        m->WriteData(p.bytes(), static_cast<int>(p.bytes_length()));
        break;
      }
      default: {
        DCHECK(p.type() == net::UploadElement::TYPE_FILE);
        WriteParam(m, p.file_path());
        WriteParam(m, p.file_range_offset());
        WriteParam(m, p.file_range_length());
        WriteParam(m, p.expected_file_modification_time());
        break;
      }
    }
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;
    switch (type) {
      case net::UploadElement::TYPE_BYTES: {
        const char* data;
        int len;
        if (!m->ReadData(iter, &data, &len))
          return false;
        r->SetToBytes(data, len);
        break;
      }
      default: {
        DCHECK(type == net::UploadElement::TYPE_FILE);
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
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<net::UploadElement>");
  }
};

void ParamTraits<scoped_refptr<net::UploadData> >::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p) {
    WriteParam(m, *p->elements());
    WriteParam(m, p->identifier());
    WriteParam(m, p->is_chunked());
    WriteParam(m, p->last_chunk_appended());
  }
}

bool ParamTraits<scoped_refptr<net::UploadData> >::Read(const Message* m,
                                                        PickleIterator* iter,
                                                        param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  std::vector<net::UploadElement> elements;
  if (!ReadParam(m, iter, &elements))
    return false;
  int64 identifier;
  if (!ReadParam(m, iter, &identifier))
    return false;
  bool is_chunked = false;
  if (!ReadParam(m, iter, &is_chunked))
    return false;
  bool last_chunk_appended = false;
  if (!ReadParam(m, iter, &last_chunk_appended))
    return false;
  *r = new net::UploadData;
  (*r)->swap_elements(&elements);
  (*r)->set_identifier(identifier);
  (*r)->set_is_chunked(is_chunked);
  (*r)->set_last_chunk_appended(last_chunk_appended);
  return true;
}

void ParamTraits<scoped_refptr<net::UploadData> >::Log(const param_type& p,
                                                       std::string* l) {
  l->append("<net::UploadData>");
}

void ParamTraits<webkit_base::DataElement>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.type()));
  switch (p.type()) {
    case webkit_base::DataElement::TYPE_BYTES: {
      m->WriteData(p.bytes(), static_cast<int>(p.length()));
      break;
    }
    case webkit_base::DataElement::TYPE_FILE: {
      WriteParam(m, p.path());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      WriteParam(m, p.expected_modification_time());
      break;
    }
    case webkit_base::DataElement::TYPE_FILE_FILESYSTEM: {
      WriteParam(m, p.url());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      WriteParam(m, p.expected_modification_time());
      break;
    }
    default: {
      DCHECK(p.type() == webkit_base::DataElement::TYPE_BLOB);
      WriteParam(m, p.url());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      break;
    }
  }
}

bool ParamTraits<webkit_base::DataElement>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;
  switch (type) {
    case webkit_base::DataElement::TYPE_BYTES: {
      const char* data;
      int len;
      if (!m->ReadData(iter, &data, &len))
        return false;
      r->SetToBytes(data, len);
      break;
    }
    case webkit_base::DataElement::TYPE_FILE: {
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
    case webkit_base::DataElement::TYPE_FILE_FILESYSTEM: {
      GURL file_system_url;
      uint64 offset, length;
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
      break;
    }
    default: {
      DCHECK(type == webkit_base::DataElement::TYPE_BLOB);
      GURL blob_url;
      uint64 offset, length;
      if (!ReadParam(m, iter, &blob_url))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      r->SetToBlobUrlRange(blob_url, offset, length);
      break;
    }
  }
  return true;
}

void ParamTraits<webkit_base::DataElement>::Log(
    const param_type& p, std::string* l) {
  l->append("<webkit_base::DataElement>");
}

void ParamTraits<scoped_refptr<webkit_glue::ResourceRequestBody> >::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p) {
    WriteParam(m, *p->elements());
    WriteParam(m, p->identifier());
  }
}

bool ParamTraits<scoped_refptr<webkit_glue::ResourceRequestBody> >::Read(
    const Message* m,
    PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  std::vector<webkit_base::DataElement> elements;
  if (!ReadParam(m, iter, &elements))
    return false;
  int64 identifier;
  if (!ReadParam(m, iter, &identifier))
    return false;
  *r = new webkit_glue::ResourceRequestBody;
  (*r)->swap_elements(&elements);
  (*r)->set_identifier(identifier);
  return true;
}

void ParamTraits<scoped_refptr<webkit_glue::ResourceRequestBody> >::Log(
    const param_type& p, std::string* l) {
  l->append("<webkit_glue::ResourceRequestBody>");
}

void ParamTraits<net::HostPortPair>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.host());
  WriteParam(m, p.port());
}

bool ParamTraits<net::HostPortPair>::Read(const Message* m,
                                          PickleIterator* iter,
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
    const Message* m, PickleIterator* iter, param_type* r) {
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

void ParamTraits<content::Referrer>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.policy);
}

bool ParamTraits<content::Referrer>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  return ReadParam(m, iter, &r->url) && ReadParam(m, iter, &r->policy);
}

void ParamTraits<content::Referrer>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.url, l);
  l->append(",");
  LogParam(p.policy, l);
  l->append(")");
}

void ParamTraits<gfx::Point>::Write(Message* m, const gfx::Point& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
}

bool ParamTraits<gfx::Point>::Read(const Message* m, PickleIterator* iter,
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

bool ParamTraits<gfx::Size>::Read(const Message* m,
                                  PickleIterator* iter,
                                  gfx::Size* r) {
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

bool ParamTraits<gfx::Rect>::Read(const Message* m,
                                  PickleIterator* iter,
                                  gfx::Rect* r) {
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

void ParamTraits<gfx::RectF>::Write(Message* m, const gfx::RectF& p) {
  ParamTraits<float>::Write(m, p.x());
  ParamTraits<float>::Write(m, p.y());
  ParamTraits<float>::Write(m, p.width());
  ParamTraits<float>::Write(m, p.height());
}

bool ParamTraits<gfx::RectF>::Read(const Message* m,
                                   PickleIterator* iter,
                                   gfx::RectF* r) {
  float x, y, w, h;
  if (!ParamTraits<float>::Read(m, iter, &x) ||
      !ParamTraits<float>::Read(m, iter, &y) ||
      !ParamTraits<float>::Read(m, iter, &w) ||
      !ParamTraits<float>::Read(m, iter, &h))
    return false;
  r->set_x(x);
  r->set_y(y);
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::RectF>::Log(const gfx::RectF& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f, %f, %f)", p.x(), p.y(),
                               p.width(), p.height()));
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

void ParamTraits<SkBitmap>::Write(Message* m, const SkBitmap& p) {
  size_t fixed_size = sizeof(SkBitmap_Data);
  SkBitmap_Data bmp_data;
  bmp_data.InitSkBitmapDataForTransfer(p);
  m->WriteData(reinterpret_cast<const char*>(&bmp_data),
               static_cast<int>(fixed_size));
  size_t pixel_size = p.getSize();
  SkAutoLockPixels p_lock(p);
  m->WriteData(reinterpret_cast<const char*>(p.getPixels()),
               static_cast<int>(pixel_size));
}

bool ParamTraits<SkBitmap>::Read(const Message* m,
                                 PickleIterator* iter,
                                 SkBitmap* r) {
  const char* fixed_data;
  int fixed_data_size = 0;
  if (!m->ReadData(iter, &fixed_data, &fixed_data_size) ||
     (fixed_data_size <= 0)) {
    NOTREACHED();
    return false;
  }
  if (fixed_data_size != sizeof(SkBitmap_Data))
    return false;  // Message is malformed.

  const char* variable_data;
  int variable_data_size = 0;
  if (!m->ReadData(iter, &variable_data, &variable_data_size) ||
     (variable_data_size < 0)) {
    NOTREACHED();
    return false;
  }
  const SkBitmap_Data* bmp_data =
      reinterpret_cast<const SkBitmap_Data*>(fixed_data);
  return bmp_data->InitSkBitmapFromData(r, variable_data, variable_data_size);
}

void ParamTraits<SkBitmap>::Log(const SkBitmap& p, std::string* l) {
  l->append("<SkBitmap>");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC
