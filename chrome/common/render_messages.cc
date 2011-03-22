// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "chrome/common/render_messages.h"
#include "chrome/common/common_param_traits.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#include "chrome/common/render_messages.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#include "chrome/common/render_messages.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "chrome/common/render_messages.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "chrome/common/render_messages.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "chrome/common/render_messages.h"
}  // namespace IPC

namespace IPC {

#if defined(OS_MACOSX)
void ParamTraits<FontDescriptor>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.font_name);
  WriteParam(m, p.font_point_size);
}

bool ParamTraits<FontDescriptor>::Read(const Message* m,
                                       void** iter,
                                       param_type* p) {
  return
      ReadParam(m, iter, &p->font_name) &&
      ReadParam(m, iter, &p->font_point_size);
}

void ParamTraits<FontDescriptor>::Log(const param_type& p, std::string* l) {
  l->append("<FontDescriptor>");
}
#endif

void ParamTraits<webkit::npapi::WebPluginGeometry>::Write(Message* m,
                                                          const param_type& p) {
  WriteParam(m, p.window);
  WriteParam(m, p.window_rect);
  WriteParam(m, p.clip_rect);
  WriteParam(m, p.cutout_rects);
  WriteParam(m, p.rects_valid);
  WriteParam(m, p.visible);
}

bool ParamTraits<webkit::npapi::WebPluginGeometry>::Read(
    const Message* m, void** iter, param_type* p) {
  return
      ReadParam(m, iter, &p->window) &&
      ReadParam(m, iter, &p->window_rect) &&
      ReadParam(m, iter, &p->clip_rect) &&
      ReadParam(m, iter, &p->cutout_rects) &&
      ReadParam(m, iter, &p->rects_valid) &&
      ReadParam(m, iter, &p->visible);
}

void ParamTraits<webkit::npapi::WebPluginGeometry>::Log(const param_type& p,
                                                        std::string* l) {
  l->append("(");
  LogParam(p.window, l);
  l->append(", ");
  LogParam(p.window_rect, l);
  l->append(", ");
  LogParam(p.clip_rect, l);
  l->append(", ");
  LogParam(p.cutout_rects, l);
  l->append(", ");
  LogParam(p.rects_valid, l);
  l->append(", ");
  LogParam(p.visible, l);
  l->append(")");
}

void ParamTraits<webkit::npapi::WebPluginMimeType>::Write(Message* m,
                                                          const param_type& p) {
  WriteParam(m, p.mime_type);
  WriteParam(m, p.file_extensions);
  WriteParam(m, p.description);
}

bool ParamTraits<webkit::npapi::WebPluginMimeType>::Read(const Message* m,
                                                         void** iter,
                                                         param_type* r) {
  return
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->file_extensions) &&
      ReadParam(m, iter, &r->description);
}

void ParamTraits<webkit::npapi::WebPluginMimeType>::Log(const param_type& p,
                                                        std::string* l) {
  l->append("(");
  LogParam(p.mime_type, l);
  l->append(", ");
  LogParam(p.file_extensions, l);
  l->append(", ");
  LogParam(p.description, l);
  l->append(")");
}

void ParamTraits<webkit::npapi::WebPluginInfo>::Write(Message* m,
                                                      const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.path);
  WriteParam(m, p.version);
  WriteParam(m, p.desc);
  WriteParam(m, p.mime_types);
  WriteParam(m, p.enabled);
}

bool ParamTraits<webkit::npapi::WebPluginInfo>::Read(const Message* m,
                                                     void** iter,
                                                     param_type* r) {
  return
      ReadParam(m, iter, &r->name) &&
      ReadParam(m, iter, &r->path) &&
      ReadParam(m, iter, &r->version) &&
      ReadParam(m, iter, &r->desc) &&
      ReadParam(m, iter, &r->mime_types) &&
      ReadParam(m, iter, &r->enabled);
}

void ParamTraits<webkit::npapi::WebPluginInfo>::Log(const param_type& p,
                                                    std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  l->append(", ");
  LogParam(p.path, l);
  l->append(", ");
  LogParam(p.version, l);
  l->append(", ");
  LogParam(p.desc, l);
  l->append(", ");
  LogParam(p.mime_types, l);
  l->append(", ");
  LogParam(p.enabled, l);
  l->append(")");
}

void ParamTraits<WebDropData>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.url_title);
  WriteParam(m, p.download_metadata);
  WriteParam(m, p.file_extension);
  WriteParam(m, p.filenames);
  WriteParam(m, p.plain_text);
  WriteParam(m, p.text_html);
  WriteParam(m, p.html_base_url);
  WriteParam(m, p.file_description_filename);
  WriteParam(m, p.file_contents);
}

bool ParamTraits<WebDropData>::Read(const Message* m, void** iter,
                                    param_type* p) {
  return
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->url_title) &&
      ReadParam(m, iter, &p->download_metadata) &&
      ReadParam(m, iter, &p->file_extension) &&
      ReadParam(m, iter, &p->filenames) &&
      ReadParam(m, iter, &p->plain_text) &&
      ReadParam(m, iter, &p->text_html) &&
      ReadParam(m, iter, &p->html_base_url) &&
      ReadParam(m, iter, &p->file_description_filename) &&
      ReadParam(m, iter, &p->file_contents);
}

void ParamTraits<WebDropData>::Log(const param_type& p, std::string* l) {
  l->append("<WebDropData>");
}

void ParamTraits<URLPattern>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.valid_schemes());
  WriteParam(m, p.GetAsString());
}

bool ParamTraits<URLPattern>::Read(const Message* m, void** iter,
                                   param_type* p) {
  int valid_schemes;
  std::string spec;
  if (!ReadParam(m, iter, &valid_schemes) ||
      !ReadParam(m, iter, &spec))
    return false;

  p->set_valid_schemes(valid_schemes);
  return URLPattern::PARSE_SUCCESS == p->Parse(spec, URLPattern::PARSE_LENIENT);
}

void ParamTraits<URLPattern>::Log(const param_type& p, std::string* l) {
  LogParam(p.GetAsString(), l);
}

void ParamTraits<webkit_glue::WebCookie>::Write(Message* m,
                                                const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.value);
  WriteParam(m, p.domain);
  WriteParam(m, p.path);
  WriteParam(m, p.expires);
  WriteParam(m, p.http_only);
  WriteParam(m, p.secure);
  WriteParam(m, p.session);
}

bool ParamTraits<webkit_glue::WebCookie>::Read(const Message* m, void** iter,
                                               param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->value) &&
      ReadParam(m, iter, &p->domain) &&
      ReadParam(m, iter, &p->path) &&
      ReadParam(m, iter, &p->expires) &&
      ReadParam(m, iter, &p->http_only) &&
      ReadParam(m, iter, &p->secure) &&
      ReadParam(m, iter, &p->session);
}

void ParamTraits<webkit_glue::WebCookie>::Log(const param_type& p,
                                              std::string* l) {
  l->append("<WebCookie>");
}

void ParamTraits<ExtensionExtent>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.patterns());
}

bool ParamTraits<ExtensionExtent>::Read(const Message* m, void** iter,
                                        param_type* p) {
  std::vector<URLPattern> patterns;
  bool success =
      ReadParam(m, iter, &patterns);
  if (!success)
    return false;

  for (size_t i = 0; i < patterns.size(); ++i)
    p->AddPattern(patterns[i]);
  return true;
}

void ParamTraits<ExtensionExtent>::Log(const param_type& p, std::string* l) {
  LogParam(p.patterns(), l);
}

void ParamTraits<webkit_glue::WebAccessibility>::Write(Message* m,
                                                       const param_type& p) {
  WriteParam(m, p.id);
  WriteParam(m, p.name);
  WriteParam(m, p.value);
  WriteParam(m, static_cast<int>(p.role));
  WriteParam(m, static_cast<int>(p.state));
  WriteParam(m, p.location);
  WriteParam(m, p.attributes);
  WriteParam(m, p.children);
  WriteParam(m, p.indirect_child_ids);
  WriteParam(m, p.html_attributes);
}

bool ParamTraits<webkit_glue::WebAccessibility>::Read(
    const Message* m, void** iter, param_type* p) {
  bool ret = ReadParam(m, iter, &p->id);
  ret = ret && ReadParam(m, iter, &p->name);
  ret = ret && ReadParam(m, iter, &p->value);
  int role = -1;
  ret = ret && ReadParam(m, iter, &role);
  if (role >= webkit_glue::WebAccessibility::ROLE_NONE &&
      role < webkit_glue::WebAccessibility::NUM_ROLES) {
    p->role = static_cast<webkit_glue::WebAccessibility::Role>(role);
  } else {
    p->role = webkit_glue::WebAccessibility::ROLE_NONE;
  }
  int state = 0;
  ret = ret && ReadParam(m, iter, &state);
  p->state = static_cast<webkit_glue::WebAccessibility::State>(state);
  ret = ret && ReadParam(m, iter, &p->location);
  ret = ret && ReadParam(m, iter, &p->attributes);
  ret = ret && ReadParam(m, iter, &p->children);
  ret = ret && ReadParam(m, iter, &p->indirect_child_ids);
  ret = ret && ReadParam(m, iter, &p->html_attributes);
  return ret;
}

void ParamTraits<webkit_glue::WebAccessibility>::Log(const param_type& p,
                                                     std::string* l) {
  l->append("(");
  LogParam(p.id, l);
  l->append(", ");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.value, l);
  l->append(", ");
  LogParam(static_cast<int>(p.role), l);
  l->append(", ");
  LogParam(static_cast<int>(p.state), l);
  l->append(", ");
  LogParam(p.location, l);
  l->append(", ");
  LogParam(p.attributes, l);
  l->append(", ");
  LogParam(p.children, l);
  l->append(", ");
  LogParam(p.html_attributes, l);
  l->append(", ");
  LogParam(p.indirect_child_ids, l);
  l->append(")");
}

}  // namespace IPC
