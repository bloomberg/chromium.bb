// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to define IPC::ParamTraits<> specializations for a number
// of WebKit types so that they can be serialized over IPC.  IPC::ParamTraits<>
// specializations for basic types (like int and std::string) and types in the
// 'base' project can be found in ipc/ipc_message_utils.h.  This file contains
// specializations for types that are used by the content code, and which need
// manual serialization code.  This is usually because they're not structs with
// public members.

#ifndef CONTENT_COMMON_WEBKIT_PARAM_TRAITS_H_
#define CONTENT_COMMON_WEBKIT_PARAM_TRAITS_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "webkit/blob/blob_data.h"
#include "webkit/glue/npruntime_util.h"
#include "webkit/glue/resource_type.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/window_open_disposition.h"

namespace webkit_glue {
struct PasswordForm;
struct ResourceDevToolsInfo;
struct ResourceLoadTimingInfo;
}

// Define the NPVariant_Param struct and its enum here since it needs manual
// serialization code.
enum NPVariant_ParamEnum {
  NPVARIANT_PARAM_VOID,
  NPVARIANT_PARAM_NULL,
  NPVARIANT_PARAM_BOOL,
  NPVARIANT_PARAM_INT,
  NPVARIANT_PARAM_DOUBLE,
  NPVARIANT_PARAM_STRING,
  // Used when when the NPObject is running in the caller's process, so we
  // create an NPObjectProxy in the other process.
  NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID,
  // Used when the NPObject we're sending is running in the callee's process
  // (i.e. we have an NPObjectProxy for it).  In that case we want the callee
  // to just use the raw pointer.
  NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID,
};

struct NPVariant_Param {
  NPVariant_Param();
  ~NPVariant_Param();

  NPVariant_ParamEnum type;
  bool bool_value;
  int int_value;
  double double_value;
  std::string string_value;
  int npobject_routing_id;
};

struct NPIdentifier_Param {
  NPIdentifier_Param();
  ~NPIdentifier_Param();

  NPIdentifier identifier;
};

namespace IPC {

template <>
struct ParamTraits<webkit_glue::ResourceLoadTimingInfo> {
  typedef webkit_glue::ResourceLoadTimingInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<webkit_glue::ResourceDevToolsInfo> > {
  typedef scoped_refptr<webkit_glue::ResourceDevToolsInfo> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<webkit_blob::BlobData > > {
  typedef scoped_refptr<webkit_blob::BlobData> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<NPVariant_Param> {
  typedef NPVariant_Param param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<NPIdentifier_Param> {
  typedef NPIdentifier_Param param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
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
struct ParamTraits<WebKit::WebInputEvent::Type> {
  typedef WebKit::WebInputEvent::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<WebKit::WebInputEvent::Type>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    const char* type;
    switch (p) {
     case WebKit::WebInputEvent::MouseDown:
      type = "MouseDown";
      break;
     case WebKit::WebInputEvent::MouseUp:
      type = "MouseUp";
      break;
     case WebKit::WebInputEvent::MouseMove:
      type = "MouseMove";
      break;
     case WebKit::WebInputEvent::MouseLeave:
      type = "MouseLeave";
      break;
     case WebKit::WebInputEvent::MouseEnter:
      type = "MouseEnter";
      break;
     case WebKit::WebInputEvent::MouseWheel:
      type = "MouseWheel";
      break;
     case WebKit::WebInputEvent::RawKeyDown:
      type = "RawKeyDown";
      break;
     case WebKit::WebInputEvent::KeyDown:
      type = "KeyDown";
      break;
     case WebKit::WebInputEvent::KeyUp:
      type = "KeyUp";
      break;
     default:
      type = "None";
      break;
    }
    LogParam(std::string(type), l);
  }
};

typedef const WebKit::WebInputEvent* WebInputEventPointer;
template <>
struct ParamTraits<WebInputEventPointer> {
  typedef WebInputEventPointer param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(p), p->size);
  }
  // Note: upon read, the event has the lifetime of the message.
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char* data;
    int data_length;
    if (!m->ReadData(iter, &data, &data_length)) {
      NOTREACHED();
      return false;
    }
    if (data_length < static_cast<int>(sizeof(WebKit::WebInputEvent))) {
      NOTREACHED();
      return false;
    }
    param_type event = reinterpret_cast<param_type>(data);
    // Check that the data size matches that of the event (we check the latter
    // in the delegate).
    if (data_length != static_cast<int>(event->size)) {
      NOTREACHED();
      return false;
    }
    *r = event;
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p->size, l);
    l->append(", ");
    LogParam(p->type, l);
    l->append(", ");
    LogParam(p->timeStampSeconds, l);
    l->append(")");
  }
};

template <>
struct SimilarTypeTraits<WebKit::WebTextDirection> {
  typedef int Type;
};

template <>
struct SimilarTypeTraits<WindowOpenDisposition> {
  typedef int Type;
};

template <>
struct CONTENT_EXPORT ParamTraits<webkit_glue::PasswordForm> {
  typedef webkit_glue::PasswordForm param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_WEBKIT_PARAM_TRAITS_H_
