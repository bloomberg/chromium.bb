// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to define IPC::ParamTraits<> specializations for a number
// of types so that they can be serialized over IPC.  IPC::ParamTraits<>
// specializations for basic types (like int and std::string) and types in the
// 'base' project can be found in ipc/ipc_message_utils.h.  This file contains
// specializations for types that are shared by more than one child process.

#ifndef CHROME_COMMON_COMMON_PARAM_TRAITS_H_
#define CHROME_COMMON_COMMON_PARAM_TRAITS_H_

#include <vector>

#include "app/gfx/native_widget_types.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/transport_dib.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/upload_data.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/window_open_disposition.h"

// Forward declarations.
class GURL;
class SkBitmap;
class DictionaryValue;
class ListValue;

namespace gfx {
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace webkit_glue {
struct WebApplicationInfo;
}  // namespace webkit_glue

namespace IPC {

template <>
struct ParamTraits<SkBitmap> {
  typedef SkBitmap param_type;
  static void Write(Message* m, const param_type& p);

  // Note: This function expects parameter |r| to be of type &SkBitmap since
  // r->SetConfig() and r->SetPixels() are called.
  static bool Read(const Message* m, void** iter, param_type* r);

  static void Log(const param_type& p, std::wstring* l);
};


template <>
struct ParamTraits<GURL> {
  typedef GURL param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};


template <>
struct ParamTraits<gfx::Point> {
  typedef gfx::Point param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<gfx::Rect> {
  typedef gfx::Rect param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<gfx::Size> {
  typedef gfx::Size param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<gfx::NativeWindow> {
  typedef gfx::NativeWindow param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    intptr_t value;
    if (!ReadParam(m, iter, &value))
      return false;
    *r = reinterpret_cast<param_type>(value);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(reinterpret_cast<intptr_t>(p), l);
  }
};


template <>
struct ParamTraits<PageZoom::Function> {
  typedef PageZoom::Function param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int value;
    if (!ReadParam(m, iter, &value))
      return false;
    *r = static_cast<param_type>(value);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(static_cast<int>(p), l);
  }
};


template <>
struct ParamTraits<WindowOpenDisposition> {
  typedef WindowOpenDisposition param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int value;
    if (!ReadParam(m, iter, &value))
      return false;
    *r = static_cast<param_type>(value);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(static_cast<int>(p), l);
  }
};


template <>
struct ParamTraits<WebCursor> {
  typedef WebCursor param_type;
  static void Write(Message* m, const param_type& p) {
    p.Serialize(m);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return r->Deserialize(m, iter);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebCursor>");
  }
};


template <>
struct ParamTraits<webkit_glue::WebApplicationInfo> {
  typedef webkit_glue::WebApplicationInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};


#if defined(OS_WIN)
template<>
struct ParamTraits<TransportDIB::Id> {
  typedef TransportDIB::Id param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.handle);
    WriteParam(m, p.sequence_num);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->handle) &&
            ReadParam(m, iter, &r->sequence_num));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"TransportDIB(");
    LogParam(p.handle, l);
    l->append(L", ");
    LogParam(p.sequence_num, l);
    l->append(L")");
  }
};
#endif

// Traits for URLRequestStatus
template <>
struct ParamTraits<URLRequestStatus> {
  typedef URLRequestStatus param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.status()));
    WriteParam(m, p.os_error());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int status, os_error;
    if (!ReadParam(m, iter, &status) ||
        !ReadParam(m, iter, &os_error))
      return false;
    r->set_status(static_cast<URLRequestStatus::Status>(status));
    r->set_os_error(os_error);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring status;
    switch (p.status()) {
     case URLRequestStatus::SUCCESS:
      status = L"SUCCESS";
      break;
     case URLRequestStatus::IO_PENDING:
      status = L"IO_PENDING ";
      break;
     case URLRequestStatus::HANDLED_EXTERNALLY:
      status = L"HANDLED_EXTERNALLY";
      break;
     case URLRequestStatus::CANCELED:
      status = L"CANCELED";
      break;
     case URLRequestStatus::FAILED:
      status = L"FAILED";
      break;
     default:
      status = L"UNKNOWN";
      break;
    }
    if (p.status() == URLRequestStatus::FAILED)
      l->append(L"(");

    LogParam(status, l);

    if (p.status() == URLRequestStatus::FAILED) {
      l->append(L", ");
      LogParam(p.os_error(), l);
      l->append(L")");
    }
  }
};


// Traits for net::UploadData::Element.
template <>
struct ParamTraits<net::UploadData::Element> {
  typedef net::UploadData::Element param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type()));
    if (p.type() == net::UploadData::TYPE_BYTES) {
      m->WriteData(&p.bytes()[0], static_cast<int>(p.bytes().size()));
    } else {
      WriteParam(m, p.file_path());
      WriteParam(m, p.file_range_offset());
      WriteParam(m, p.file_range_length());
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;
    if (type == net::UploadData::TYPE_BYTES) {
      const char* data;
      int len;
      if (!m->ReadData(iter, &data, &len))
        return false;
      r->SetToBytes(data, len);
    } else {
      DCHECK(type == net::UploadData::TYPE_FILE);
      FilePath file_path;
      uint64 offset, length;
      if (!ReadParam(m, iter, &file_path))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      r->SetToFilePathRange(file_path, offset, length);
    }
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<net::UploadData::Element>");
  }
};

// Traits for net::UploadData.
template <>
struct ParamTraits<scoped_refptr<net::UploadData> > {
  typedef scoped_refptr<net::UploadData> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.get() != NULL);
    if (p) {
      WriteParam(m, p->elements());
      WriteParam(m, p->identifier());
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
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
    *r = new net::UploadData;
    (*r)->swap_elements(&elements);
    (*r)->set_identifier(identifier);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<net::UploadData>");
  }
};

template<>
struct ParamTraits<ThumbnailScore> {
  typedef ThumbnailScore param_type;
  static void Write(Message* m, const param_type& p) {
    IPC::ParamTraits<double>::Write(m, p.boring_score);
    IPC::ParamTraits<bool>::Write(m, p.good_clipping);
    IPC::ParamTraits<bool>::Write(m, p.at_top);
    IPC::ParamTraits<base::Time>::Write(m, p.time_at_snapshot);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    double boring_score;
    bool good_clipping, at_top;
    base::Time time_at_snapshot;
    if (!IPC::ParamTraits<double>::Read(m, iter, &boring_score) ||
        !IPC::ParamTraits<bool>::Read(m, iter, &good_clipping) ||
        !IPC::ParamTraits<bool>::Read(m, iter, &at_top) ||
        !IPC::ParamTraits<base::Time>::Read(m, iter, &time_at_snapshot))
      return false;

    r->boring_score = boring_score;
    r->good_clipping = good_clipping;
    r->at_top = at_top;
    r->time_at_snapshot = time_at_snapshot;
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"(%f, %d, %d)",
                           p.boring_score, p.good_clipping, p.at_top));
  }
};

}  // namespace IPC

#endif  // CHROME_COMMON_COMMON_PARAM_TRAITS_H_
