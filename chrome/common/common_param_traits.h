// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to define IPC::ParamTraits<> specializations for a number
// of types so that they can be serialized over IPC.  IPC::ParamTraits<>
// specializations for basic types (like int and std::string) and types in the
// 'base' project can be found in ipc/ipc_message_utils.h.  This file contains
// specializations for types that are shared by more than one child process.

#ifndef CHROME_COMMON_COMMON_PARAM_TRAITS_H_
#define CHROME_COMMON_COMMON_PARAM_TRAITS_H_
#pragma once

#include <vector>

#include "app/surface/transport_dib.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/geoposition.h"
#include "chrome/common/page_zoom.h"
#include "gfx/native_widget_types.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/upload_data.h"
#include "net/url_request/url_request_status.h"
#include "printing/native_metafile.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/window_open_disposition.h"

// Forward declarations.
class GURL;
class SkBitmap;
class DictionaryValue;
class ListValue;
struct ThumbnailScore;
class URLRequestStatus;
class WebCursor;

namespace gfx {
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace printing {
struct PageRange;
}  // namespace printing

namespace webkit_glue {
struct PasswordForm;
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

  static void Log(const param_type& p, std::string* l);
};


template <>
struct ParamTraits<GURL> {
  typedef GURL param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};


template <>
struct ParamTraits<gfx::Point> {
  typedef gfx::Point param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<gfx::Rect> {
  typedef gfx::Rect param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<gfx::Size> {
  typedef gfx::Size param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ContentSetting> {
  typedef ContentSetting param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ContentSettingsType> {
  typedef ContentSettingsType param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int value;
    if (!ReadParam(m, iter, &value))
      return false;
    if (value < 0 || value >= static_cast<int>(CONTENT_SETTINGS_NUM_TYPES))
      return false;
    *r = static_cast<param_type>(value);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(static_cast<int>(p), l);
  }
};

template <>
struct ParamTraits<ContentSettings> {
  typedef ContentSettings param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<gfx::NativeWindow> {
  typedef gfx::NativeWindow param_type;
  static void Write(Message* m, const param_type& p) {
#if defined(OS_WIN)
    // HWNDs are always 32 bits on Windows, even on 64 bit systems.
    m->WriteUInt32(reinterpret_cast<uint32>(p));
#else
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(p));
#endif
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
#if defined(OS_WIN)
    return m->ReadUInt32(iter, reinterpret_cast<uint32*>(r));
#else
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(gfx::NativeWindow)) {
      memcpy(r, data, sizeof(gfx::NativeWindow));
    } else {
      result = false;
      NOTREACHED();
    }
    return result;
#endif
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<gfx::NativeWindow>");
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
  static void Log(const param_type& p, std::string* l) {
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
  static void Log(const param_type& p, std::string* l) {
    LogParam(static_cast<int>(p), l);
  }
};


template <>
struct ParamTraits<WebCursor> {
  typedef WebCursor param_type;
  static void Write(Message* m, const param_type& p) {
    p.Serialize(m);
  }
  static bool Read(const Message* m, void** iter, param_type* r)  {
    return r->Deserialize(m, iter);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<WebCursor>");
  }
};


template <>
struct ParamTraits<webkit_glue::WebApplicationInfo> {
  typedef webkit_glue::WebApplicationInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
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
  static void Log(const param_type& p, std::string* l) {
    l->append("TransportDIB(");
    LogParam(p.handle, l);
    l->append(", ");
    LogParam(p.sequence_num, l);
    l->append(")");
  }
};
#endif

// Traits for URLRequestStatus
template <>
struct ParamTraits<URLRequestStatus> {
  typedef URLRequestStatus param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
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
      WriteParam(m, p.expected_file_modification_time());
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
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<net::UploadData::Element>");
  }
};

// Traits for net::UploadData.
template <>
struct ParamTraits<scoped_refptr<net::UploadData> > {
  typedef scoped_refptr<net::UploadData> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.get() != NULL);
    if (p) {
      WriteParam(m, *p->elements());
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
  static void Log(const param_type& p, std::string* l) {
    l->append("<net::UploadData>");
  }
};

template<>
struct ParamTraits<ThumbnailScore> {
  typedef ThumbnailScore param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<Geoposition> {
  typedef Geoposition param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<Geoposition::ErrorCode> {
  typedef Geoposition::ErrorCode param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit_glue::PasswordForm> {
  typedef webkit_glue::PasswordForm param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<printing::PageRange> {
  typedef printing::PageRange param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<printing::NativeMetafile> {
  typedef printing::NativeMetafile param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_COMMON_PARAM_TRAITS_H_
