// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and plugin process, as well as between
// the renderer and plugin process.
//
// See render_message* for information about the multi-pass include of headers.

#ifndef CHROME_COMMON_PLUGIN_MESSAGES_H_
#define CHROME_COMMON_PLUGIN_MESSAGES_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/webkit_param_traits.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/npruntime_util.h"

// Name prefix of the event handle when a message box is displayed.
#define kMessageBoxEventPrefix L"message_box_active"

// Structures for messages that have too many parameters to be put in a
// predefined IPC message.

struct PluginMsg_Init_Params {
  PluginMsg_Init_Params();
  ~PluginMsg_Init_Params();

  gfx::NativeViewId containing_window;
  GURL url;
  GURL page_url;
  std::vector<std::string> arg_names;
  std::vector<std::string> arg_values;
  bool load_manually;
  int host_render_view_routing_id;
};

struct PluginHostMsg_URLRequest_Params {
  PluginHostMsg_URLRequest_Params();
  ~PluginHostMsg_URLRequest_Params();

  std::string url;
  std::string method;
  std::string target;
  std::vector<char> buffer;
  int notify_id;
  bool popups_allowed;
  bool notify_redirects;
};

struct PluginMsg_DidReceiveResponseParams {
  PluginMsg_DidReceiveResponseParams();
  ~PluginMsg_DidReceiveResponseParams();

  unsigned long id;
  std::string mime_type;
  std::string headers;
  uint32 expected_length;
  uint32 last_modified;
  bool request_is_seekable;
};

struct NPIdentifier_Param {
  NPIdentifier_Param();
  ~NPIdentifier_Param();

  NPIdentifier identifier;
};

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

struct PluginMsg_UpdateGeometry_Param {
  PluginMsg_UpdateGeometry_Param();
  ~PluginMsg_UpdateGeometry_Param();

  gfx::Rect window_rect;
  gfx::Rect clip_rect;
  bool transparent;
  TransportDIB::Handle windowless_buffer;
  TransportDIB::Handle background_buffer;

#if defined(OS_MACOSX)
  // This field contains a key that the plug-in process is expected to return
  // to the renderer in its ACK message, unless the value is -1, in which case
  // no ACK message is required.  Other than the special -1 value, the values
  // used in ack_key are opaque to the plug-in process.
  int ack_key;
#endif
};


namespace IPC {

// Traits for PluginMsg_Init_Params structure to pack/unpack.
template <>
struct ParamTraits<PluginMsg_Init_Params> {
  typedef PluginMsg_Init_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<PluginHostMsg_URLRequest_Params> {
  typedef PluginHostMsg_URLRequest_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<PluginMsg_DidReceiveResponseParams> {
  typedef PluginMsg_DidReceiveResponseParams param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
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
struct ParamTraits<NPIdentifier_Param> {
  typedef NPIdentifier_Param param_type;
  static void Write(Message* m, const param_type& p) {
    webkit_glue::SerializeNPIdentifier(p.identifier, m);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return webkit_glue::DeserializeNPIdentifier(*m, iter, &r->identifier);
  }
  static void Log(const param_type& p, std::string* l) {
    if (WebKit::WebBindings::identifierIsString(p.identifier)) {
      NPUTF8* str = WebKit::WebBindings::utf8FromIdentifier(p.identifier);
      l->append(str);
      NPN_MemFree(str);
    } else {
      l->append(base::IntToString(
          WebKit::WebBindings::intFromIdentifier(p.identifier)));
    }
  }
};

template <>
struct ParamTraits<NPVariant_Param> {
  typedef NPVariant_Param param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// For windowless plugins, windowless_buffer
// contains a buffer that the plugin draws into.  background_buffer is used
// for transparent windowless plugins, and holds the background of the plugin
// rectangle.
template <>
struct ParamTraits<PluginMsg_UpdateGeometry_Param> {
  typedef PluginMsg_UpdateGeometry_Param param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#include "chrome/common/plugin_messages_internal.h"

#endif  // CHROME_COMMON_PLUGIN_MESSAGES_H_
