// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "content/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/plugin_messages.h"

PluginMsg_Init_Params::PluginMsg_Init_Params()
    : containing_window(0),
      load_manually(false),
      host_render_view_routing_id(-1) {
}

PluginMsg_Init_Params::~PluginMsg_Init_Params() {
}

PluginHostMsg_URLRequest_Params::PluginHostMsg_URLRequest_Params()
    : notify_id(-1),
      popups_allowed(false),
      notify_redirects(false) {
}

PluginHostMsg_URLRequest_Params::~PluginHostMsg_URLRequest_Params() {
}

PluginMsg_DidReceiveResponseParams::PluginMsg_DidReceiveResponseParams()
    : id(-1),
      expected_length(0),
      last_modified(0),
      request_is_seekable(false) {
}

PluginMsg_DidReceiveResponseParams::~PluginMsg_DidReceiveResponseParams() {
}

NPIdentifier_Param::NPIdentifier_Param()
    : identifier() {
}

NPIdentifier_Param::~NPIdentifier_Param() {
}

NPVariant_Param::NPVariant_Param()
    : type(NPVARIANT_PARAM_VOID),
      bool_value(false),
      int_value(0),
      double_value(0),
      npobject_routing_id(-1) {
}

NPVariant_Param::~NPVariant_Param() {
}

PluginMsg_UpdateGeometry_Param::PluginMsg_UpdateGeometry_Param()
    : transparent(false),
#if !defined(OS_MACOSX)
      windowless_buffer(TransportDIB::DefaultHandleValue()),
      background_buffer(TransportDIB::DefaultHandleValue())
#else
      ack_key(-1)
#endif  // !defined(OS_MACOSX)
{
}

PluginMsg_UpdateGeometry_Param::~PluginMsg_UpdateGeometry_Param() {
}

namespace IPC {

void ParamTraits<PluginMsg_Init_Params>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.containing_window);
  WriteParam(m, p.url);
  WriteParam(m, p.page_url);
  DCHECK(p.arg_names.size() == p.arg_values.size());
  WriteParam(m, p.arg_names);
  WriteParam(m, p.arg_values);
  WriteParam(m, p.load_manually);
  WriteParam(m, p.host_render_view_routing_id);
}

bool ParamTraits<PluginMsg_Init_Params>::Read(const Message* m,
                                              void** iter,
                                              param_type* p) {
  return ReadParam(m, iter, &p->containing_window) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->page_url) &&
      ReadParam(m, iter, &p->arg_names) &&
      ReadParam(m, iter, &p->arg_values) &&
      ReadParam(m, iter, &p->load_manually) &&
      ReadParam(m, iter, &p->host_render_view_routing_id);
}

void ParamTraits<PluginMsg_Init_Params>::Log(const param_type& p,
                                             std::string* l) {
  l->append("(");
  LogParam(p.containing_window, l);
  l->append(", ");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.page_url, l);
  l->append(", ");
  LogParam(p.arg_names, l);
  l->append(", ");
  LogParam(p.arg_values, l);
  l->append(", ");
  LogParam(p.load_manually, l);
  l->append(", ");
  LogParam(p.host_render_view_routing_id, l);
  l->append(")");
}

void ParamTraits<PluginHostMsg_URLRequest_Params>::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.method);
  WriteParam(m, p.target);
  WriteParam(m, p.buffer);
  WriteParam(m, p.notify_id);
  WriteParam(m, p.popups_allowed);
  WriteParam(m, p.notify_redirects);
}

bool ParamTraits<PluginHostMsg_URLRequest_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  return
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->method) &&
      ReadParam(m, iter, &p->target) &&
      ReadParam(m, iter, &p->buffer) &&
      ReadParam(m, iter, &p->notify_id) &&
      ReadParam(m, iter, &p->popups_allowed) &&
      ReadParam(m, iter, &p->notify_redirects);
}

void ParamTraits<PluginHostMsg_URLRequest_Params>::Log(const param_type& p,
                                                       std::string* l) {
  l->append("(");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.method, l);
  l->append(", ");
  LogParam(p.target, l);
  l->append(", ");
  LogParam(p.buffer, l);
  l->append(", ");
  LogParam(p.notify_id, l);
  l->append(", ");
  LogParam(p.popups_allowed, l);
  l->append(", ");
  LogParam(p.notify_redirects, l);
  l->append(")");
}


void ParamTraits<PluginMsg_DidReceiveResponseParams>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.id);
  WriteParam(m, p.mime_type);
  WriteParam(m, p.headers);
  WriteParam(m, p.expected_length);
  WriteParam(m, p.last_modified);
  WriteParam(m, p.request_is_seekable);
}

bool ParamTraits<PluginMsg_DidReceiveResponseParams>::Read(const Message* m,
                                                           void** iter,
                                                           param_type* r) {
  return
      ReadParam(m, iter, &r->id) &&
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->expected_length) &&
      ReadParam(m, iter, &r->last_modified) &&
      ReadParam(m, iter, &r->request_is_seekable);
}

void ParamTraits<PluginMsg_DidReceiveResponseParams>::Log(const param_type& p,
                                                          std::string* l) {
  l->append("(");
  LogParam(p.id, l);
  l->append(", ");
  LogParam(p.mime_type, l);
  l->append(", ");
  LogParam(p.headers, l);
  l->append(", ");
  LogParam(p.expected_length, l);
  l->append(", ");
  LogParam(p.last_modified, l);
  l->append(", ");
  LogParam(p.request_is_seekable, l);
  l->append(")");
}


void ParamTraits<NPVariant_Param>::Write(Message* m, const param_type& p) {
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

bool ParamTraits<NPVariant_Param>::Read(const Message* m,
                                        void** iter,
                                        param_type* r) {
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

void ParamTraits<NPVariant_Param>::Log(const param_type& p, std::string* l) {
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

void ParamTraits<PluginMsg_UpdateGeometry_Param>::Write(Message* m,
                                                        const param_type& p) {
  WriteParam(m, p.window_rect);
  WriteParam(m, p.clip_rect);
  WriteParam(m, p.windowless_buffer);
  WriteParam(m, p.background_buffer);
  WriteParam(m, p.transparent);
#if defined(OS_MACOSX)
  WriteParam(m, p.ack_key);
#endif
}

bool ParamTraits<PluginMsg_UpdateGeometry_Param>::Read(const Message* m,
                                                       void** iter,
                                                       param_type* r) {
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

void ParamTraits<PluginMsg_UpdateGeometry_Param>::Log(const param_type& p,
                                                      std::string* l) {
  l->append("(");
  LogParam(p.window_rect, l);
  l->append(", ");
  LogParam(p.clip_rect, l);
  l->append(", ");
  LogParam(p.windowless_buffer, l);
  l->append(", ");
  LogParam(p.background_buffer, l);
  l->append(", ");
  LogParam(p.transparent, l);
#if defined(OS_MACOSX)
  l->append(", ");
  LogParam(p.ack_key, l);
#endif
  l->append(")");
}

}  // namespace IPC
