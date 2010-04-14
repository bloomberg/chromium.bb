// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and plugin process, as well as between
// the renderer and plugin process.
//
// See render_message* for information about the multi-pass include of headers.

#ifndef CHROME_COMMON_PLUGIN_MESSAGES_H_
#define CHROME_COMMON_PLUGIN_MESSAGES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/webkit_param_traits.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/glue/npruntime_util.h"

// Name prefix of the event handle when a message box is displayed.
#define kMessageBoxEventPrefix L"message_box_active"

// Structures for messages that have too many parameters to be put in a
// predefined IPC message.

struct PluginMsg_Init_Params {
  gfx::NativeViewId containing_window;
  GURL url;
  GURL page_url;
  std::vector<std::string> arg_names;
  std::vector<std::string> arg_values;
  bool load_manually;
  int host_render_view_routing_id;
#if defined(OS_MACOSX)
  gfx::Rect containing_window_frame;
  gfx::Rect containing_content_frame;
  bool containing_window_has_focus;
#endif
};

struct PluginHostMsg_URLRequest_Params {
  std::string url;
  std::string method;
  std::string target;
  std::vector<char> buffer;
  int notify_id;
  bool popups_allowed;
};

struct PluginMsg_DidReceiveResponseParams {
  unsigned long id;
  std::string mime_type;
  std::string headers;
  uint32 expected_length;
  uint32 last_modified;
  bool request_is_seekable;
};

struct NPIdentifier_Param {
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
  NPVariant_ParamEnum type;
  bool bool_value;
  int int_value;
  double double_value;
  std::string string_value;
  int npobject_routing_id;
};

struct PluginMsg_UpdateGeometry_Param {
  gfx::Rect window_rect;
  gfx::Rect clip_rect;
  TransportDIB::Handle windowless_buffer;
  TransportDIB::Handle background_buffer;
  bool transparent;

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
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.containing_window);
    WriteParam(m, p.url);
    WriteParam(m, p.page_url);
    DCHECK(p.arg_names.size() == p.arg_values.size());
    WriteParam(m, p.arg_names);
    WriteParam(m, p.arg_values);
    WriteParam(m, p.load_manually);
    WriteParam(m, p.host_render_view_routing_id);
#if defined(OS_MACOSX)
    WriteParam(m, p.containing_window_frame);
    WriteParam(m, p.containing_content_frame);
    WriteParam(m, p.containing_window_has_focus);
#endif
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->containing_window) &&
           ReadParam(m, iter, &p->url) &&
           ReadParam(m, iter, &p->page_url) &&
           ReadParam(m, iter, &p->arg_names) &&
           ReadParam(m, iter, &p->arg_values) &&
           ReadParam(m, iter, &p->load_manually) &&
           ReadParam(m, iter, &p->host_render_view_routing_id)
#if defined(OS_MACOSX)
           &&
           ReadParam(m, iter, &p->containing_window_frame) &&
           ReadParam(m, iter, &p->containing_content_frame) &&
           ReadParam(m, iter, &p->containing_window_has_focus)
#endif
           ;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.containing_window, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.page_url, l);
    l->append(L", ");
    LogParam(p.arg_names, l);
    l->append(L", ");
    LogParam(p.arg_values, l);
    l->append(L", ");
    LogParam(p.load_manually, l);
    l->append(L", ");
    LogParam(p.host_render_view_routing_id, l);
#if defined(OS_MACOSX)
    l->append(L", ");
    LogParam(p.containing_window_frame, l);
    l->append(L", ");
    LogParam(p.containing_content_frame, l);
    l->append(L", ");
    LogParam(p.containing_window_has_focus, l);
#endif
    l->append(L")");
  }
};

template <>
struct ParamTraits<PluginHostMsg_URLRequest_Params> {
  typedef PluginHostMsg_URLRequest_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url);
    WriteParam(m, p.method);
    WriteParam(m, p.target);
    WriteParam(m, p.buffer);
    WriteParam(m, p.notify_id);
    WriteParam(m, p.popups_allowed);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->method) &&
      ReadParam(m, iter, &p->target) &&
      ReadParam(m, iter, &p->buffer) &&
      ReadParam(m, iter, &p->notify_id) &&
      ReadParam(m, iter, &p->popups_allowed);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.method, l);
    l->append(L", ");
    LogParam(p.target, l);
    l->append(L", ");
    LogParam(p.buffer, l);
    l->append(L", ");
    LogParam(p.notify_id, l);
    l->append(L", ");
    LogParam(p.popups_allowed, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<PluginMsg_DidReceiveResponseParams> {
  typedef PluginMsg_DidReceiveResponseParams param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.id);
    WriteParam(m, p.mime_type);
    WriteParam(m, p.headers);
    WriteParam(m, p.expected_length);
    WriteParam(m, p.last_modified);
    WriteParam(m, p.request_is_seekable);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->id) &&
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->expected_length) &&
      ReadParam(m, iter, &r->last_modified) &&
      ReadParam(m, iter, &r->request_is_seekable);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.id, l);
    l->append(L", ");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.headers, l);
    l->append(L", ");
    LogParam(p.expected_length, l);
    l->append(L", ");
    LogParam(p.last_modified, l);
    l->append(L", ");
    LogParam(p.request_is_seekable, l);
    l->append(L")");
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
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p->size, l);
    l->append(L", ");
    LogParam(p->type, l);
    l->append(L", ");
    LogParam(p->timeStampSeconds, l);
    l->append(L")");
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
  static void Log(const param_type& p, std::wstring* l) {
    if (WebKit::WebBindings::identifierIsString(p.identifier)) {
      NPUTF8* str = WebKit::WebBindings::utf8FromIdentifier(p.identifier);
      l->append(UTF8ToWide(str));
      NPN_MemFree(str);
    } else {
      l->append(IntToWString(
          WebKit::WebBindings::intFromIdentifier(p.identifier)));
    }
  }
};

template <>
struct ParamTraits<NPVariant_Param> {
  typedef NPVariant_Param param_type;
  static void Write(Message* m, const param_type& p) {
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
  static bool Read(const Message* m, void** iter, param_type* r) {
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
  static void Log(const param_type& p, std::wstring* l) {
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
    }
  }
};

// For windowless plugins, windowless_buffer
// contains a buffer that the plugin draws into.  background_buffer is used
// for transparent windowless plugins, and holds the background of the plugin
// rectangle.
template <>
struct ParamTraits<PluginMsg_UpdateGeometry_Param> {
  typedef PluginMsg_UpdateGeometry_Param param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.window_rect);
    WriteParam(m, p.clip_rect);
    WriteParam(m, p.windowless_buffer);
    WriteParam(m, p.background_buffer);
    WriteParam(m, p.transparent);
#if defined(OS_MACOSX)
    WriteParam(m, p.ack_key);
#endif
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->window_rect) &&
      ReadParam(m, iter, &r->clip_rect) &&
      ReadParam(m, iter, &r->windowless_buffer) &&
      ReadParam(m, iter, &r->background_buffer) &&
      ReadParam(m, iter, &r->transparent)
#if defined(OS_MACOSX)
      &&
      ReadParam(m, iter, &r->ack_key)
#endif
      ;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.window_rect, l);
    l->append(L", ");
    LogParam(p.clip_rect, l);
    l->append(L", ");
    LogParam(p.windowless_buffer, l);
    l->append(L", ");
    LogParam(p.background_buffer, l);
    l->append(L", ");
    LogParam(p.transparent, l);
#if defined(OS_MACOSX)
    l->append(L", ");
    LogParam(p.ack_key, l);
#endif
    l->append(L")");
  }
};

}  // namespace IPC


#define MESSAGES_INTERNAL_FILE "chrome/common/plugin_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_PLUGIN_MESSAGES_H_
