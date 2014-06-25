// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to define IPC::ParamTraits<> specializations for a number
// of types so that they can be serialized over IPC.  IPC::ParamTraits<>
// specializations for basic types (like int and std::string) and types in the
// 'base' project can be found in ipc/ipc_message_utils.h.  This file contains
// specializations for types that are used by the content code, and which need
// manual serialization code.  This is usually because they're not structs with
// public members, or because the same type is being used in multiple
// *_messages.h headers.

#ifndef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_H_
#define CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class PageState;
}

namespace net {
class HostPortPair;
class IPEndPoint;
}

namespace IPC {

template <>
struct CONTENT_EXPORT ParamTraits<GURL> {
  typedef GURL param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<url::Origin> {
  typedef url::Origin param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct CONTENT_EXPORT ParamTraits<net::HostPortPair> {
  typedef net::HostPortPair param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<net::IPEndPoint> {
  typedef net::IPEndPoint param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<content::PageState> {
  typedef content::PageState param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
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
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
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

#if defined(OS_WIN)
template<>
struct ParamTraits<TransportDIB::Id> {
  typedef TransportDIB::Id param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.handle);
    WriteParam(m, p.sequence_num);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
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

}  // namespace IPC

#endif  // CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_H_
