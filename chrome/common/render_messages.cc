// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"

#include "base/values.h"
#include "chrome/common/edit_command.h"
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/gpu_param_traits.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/indexed_db_param_traits.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/serialized_script_value.h"
#include "chrome/common/thumbnail_score.h"
#include "gfx/rect.h"
#include "ipc/ipc_channel_handle.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/blob/blob_data.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webcookie.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/plugins/webplugin.h"
#include "webkit/glue/plugins/webplugininfo.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/webmenuitem.h"

#if defined(OS_MACOSX)
#include "chrome/common/font_descriptor_mac.h"
#endif

#define MESSAGES_INTERNAL_IMPL_FILE \
  "chrome/common/render_messages_internal.h"
#include "ipc/ipc_message_impl_macros.h"

namespace IPC {


template<>
struct ParamTraits<WebMenuItem::Type> {
  typedef WebMenuItem::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<WebMenuItem::Type>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string type;
    switch (p) {
      case WebMenuItem::OPTION:
        type = "OPTION";
        break;
      case WebMenuItem::GROUP:
        type = "GROUP";
        break;
      case WebMenuItem::SEPARATOR:
        type = "SEPARATOR";
        break;
      default:
        type = "UNKNOWN";
        break;
    }
    LogParam(type, l);
  }
};


void ParamTraits<webkit_glue::FormField>::Write(Message* m,
                                                const param_type& p) {
  WriteParam(m, p.label());
  WriteParam(m, p.name());
  WriteParam(m, p.value());
  WriteParam(m, p.form_control_type());
  WriteParam(m, p.size());
  WriteParam(m, p.option_strings());
}

bool ParamTraits<webkit_glue::FormField>::Read(const Message* m, void** iter,
                                               param_type* p) {
  string16 label, name, value, form_control_type;
  int size = 0;
  std::vector<string16> options;
  bool result = ReadParam(m, iter, &label);
  result = result && ReadParam(m, iter, &name);
  result = result && ReadParam(m, iter, &value);
  result = result && ReadParam(m, iter, &form_control_type);
  result = result && ReadParam(m, iter, &size);
  result = result && ReadParam(m, iter, &options);
  if (!result)
    return false;

  p->set_label(label);
  p->set_name(name);
  p->set_value(value);
  p->set_form_control_type(form_control_type);
  p->set_size(size);
  p->set_option_strings(options);
  return true;
}

void ParamTraits<webkit_glue::FormField>::Log(const param_type& p,
                                              std::string* l) {
  l->append("<FormField>");
}

#if defined(OS_MACOSX)
void ParamTraits<FontDescriptor>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.font_name);
  WriteParam(m, p.font_point_size);
}

bool ParamTraits<FontDescriptor>::Read(const Message* m,
                                       void** iter,
                                       param_type* p) {
  return(
      ReadParam(m, iter, &p->font_name) &&
      ReadParam(m, iter, &p->font_point_size));
}

void ParamTraits<FontDescriptor>::Log(const param_type& p, std::string* l) {
  l->append("<FontDescriptor>");
}
#endif

void ParamTraits<ContextMenuParams>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.media_type);
  WriteParam(m, p.x);
  WriteParam(m, p.y);
  WriteParam(m, p.link_url);
  WriteParam(m, p.unfiltered_link_url);
  WriteParam(m, p.src_url);
  WriteParam(m, p.is_image_blocked);
  WriteParam(m, p.page_url);
  WriteParam(m, p.frame_url);
  WriteParam(m, p.media_flags);
  WriteParam(m, p.selection_text);
  WriteParam(m, p.misspelled_word);
  WriteParam(m, p.dictionary_suggestions);
  WriteParam(m, p.spellcheck_enabled);
  WriteParam(m, p.is_editable);
#if defined(OS_MACOSX)
  WriteParam(m, p.writing_direction_default);
  WriteParam(m, p.writing_direction_left_to_right);
  WriteParam(m, p.writing_direction_right_to_left);
#endif  // OS_MACOSX
  WriteParam(m, p.edit_flags);
  WriteParam(m, p.security_info);
  WriteParam(m, p.frame_charset);
  WriteParam(m, p.custom_items);
}

bool ParamTraits<ContextMenuParams>::Read(const Message* m, void** iter,
                                          param_type* p) {
  return
      ReadParam(m, iter, &p->media_type) &&
      ReadParam(m, iter, &p->x) &&
      ReadParam(m, iter, &p->y) &&
      ReadParam(m, iter, &p->link_url) &&
      ReadParam(m, iter, &p->unfiltered_link_url) &&
      ReadParam(m, iter, &p->src_url) &&
      ReadParam(m, iter, &p->is_image_blocked) &&
      ReadParam(m, iter, &p->page_url) &&
      ReadParam(m, iter, &p->frame_url) &&
      ReadParam(m, iter, &p->media_flags) &&
      ReadParam(m, iter, &p->selection_text) &&
      ReadParam(m, iter, &p->misspelled_word) &&
      ReadParam(m, iter, &p->dictionary_suggestions) &&
      ReadParam(m, iter, &p->spellcheck_enabled) &&
      ReadParam(m, iter, &p->is_editable) &&
#if defined(OS_MACOSX)
      ReadParam(m, iter, &p->writing_direction_default) &&
      ReadParam(m, iter, &p->writing_direction_left_to_right) &&
      ReadParam(m, iter, &p->writing_direction_right_to_left) &&
#endif  // OS_MACOSX
      ReadParam(m, iter, &p->edit_flags) &&
      ReadParam(m, iter, &p->security_info) &&
      ReadParam(m, iter, &p->frame_charset) &&
      ReadParam(m, iter, &p->custom_items);
}

void ParamTraits<ContextMenuParams>::Log(const param_type& p,
                                         std::string* l) {
  l->append("<ContextMenuParams>");
}

void ParamTraits<webkit_glue::WebPluginGeometry>::Write(Message* m,
                                                        const param_type& p) {
  WriteParam(m, p.window);
  WriteParam(m, p.window_rect);
  WriteParam(m, p.clip_rect);
  WriteParam(m, p.cutout_rects);
  WriteParam(m, p.rects_valid);
  WriteParam(m, p.visible);
}

bool ParamTraits<webkit_glue::WebPluginGeometry>::Read(
    const Message* m, void** iter, param_type* p) {
  return
      ReadParam(m, iter, &p->window) &&
      ReadParam(m, iter, &p->window_rect) &&
      ReadParam(m, iter, &p->clip_rect) &&
      ReadParam(m, iter, &p->cutout_rects) &&
      ReadParam(m, iter, &p->rects_valid) &&
      ReadParam(m, iter, &p->visible);
}

void ParamTraits<webkit_glue::WebPluginGeometry>::Log(const param_type& p,
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

void ParamTraits<WebPluginMimeType>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.mime_type);
  WriteParam(m, p.file_extensions);
  WriteParam(m, p.description);
}

bool ParamTraits<WebPluginMimeType>::Read(const Message* m, void** iter,
                                          param_type* r) {
  return
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->file_extensions) &&
      ReadParam(m, iter, &r->description);
}

void ParamTraits<WebPluginMimeType>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.mime_type, l);
  l->append(", ");
  LogParam(p.file_extensions, l);
  l->append(", ");
  LogParam(p.description, l);
  l->append(")");
}

void ParamTraits<WebPluginInfo>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.path);
  WriteParam(m, p.version);
  WriteParam(m, p.desc);
  WriteParam(m, p.mime_types);
  WriteParam(m, p.enabled);
}

bool ParamTraits<WebPluginInfo>::Read(const Message* m, void** iter,
                                      param_type* r) {
  return
      ReadParam(m, iter, &r->name) &&
      ReadParam(m, iter, &r->path) &&
      ReadParam(m, iter, &r->version) &&
      ReadParam(m, iter, &r->desc) &&
      ReadParam(m, iter, &r->mime_types) &&
      ReadParam(m, iter, &r->enabled);
}

void ParamTraits<WebPluginInfo>::Log(const param_type& p, std::string* l) {
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

void ParamTraits<webkit_glue::PasswordFormFillData>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.basic_data);
  WriteParam(m, p.additional_logins);
  WriteParam(m, p.wait_for_username);
}

bool ParamTraits<webkit_glue::PasswordFormFillData>::Read(
    const Message* m, void** iter, param_type* r) {
  return
      ReadParam(m, iter, &r->basic_data) &&
      ReadParam(m, iter, &r->additional_logins) &&
      ReadParam(m, iter, &r->wait_for_username);
}

void ParamTraits<webkit_glue::PasswordFormFillData>::Log(const param_type& p,
                                                         std::string* l) {
  l->append("<PasswordFormFillData>");
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

void ParamTraits<webkit_glue::FormData>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.method);
  WriteParam(m, p.origin);
  WriteParam(m, p.action);
  WriteParam(m, p.user_submitted);
  WriteParam(m, p.fields);
}

bool ParamTraits<webkit_glue::FormData>::Read(const Message* m, void** iter,
                                              param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->method) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->user_submitted) &&
      ReadParam(m, iter, &p->fields);
}

void ParamTraits<webkit_glue::FormData>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<FormData>");
}

void ParamTraits<RendererPreferences>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.can_accept_load_drops);
  WriteParam(m, p.should_antialias_text);
  WriteParam(m, static_cast<int>(p.hinting));
  WriteParam(m, static_cast<int>(p.subpixel_rendering));
  WriteParam(m, p.focus_ring_color);
  WriteParam(m, p.thumb_active_color);
  WriteParam(m, p.thumb_inactive_color);
  WriteParam(m, p.track_color);
  WriteParam(m, p.active_selection_bg_color);
  WriteParam(m, p.active_selection_fg_color);
  WriteParam(m, p.inactive_selection_bg_color);
  WriteParam(m, p.inactive_selection_fg_color);
  WriteParam(m, p.browser_handles_top_level_requests);
  WriteParam(m, p.caret_blink_interval);
}

bool ParamTraits<RendererPreferences>::Read(const Message* m, void** iter,
                                            param_type* p) {
  if (!ReadParam(m, iter, &p->can_accept_load_drops))
    return false;
  if (!ReadParam(m, iter, &p->should_antialias_text))
    return false;

  int hinting = 0;
  if (!ReadParam(m, iter, &hinting))
    return false;
  p->hinting = static_cast<RendererPreferencesHintingEnum>(hinting);

  int subpixel_rendering = 0;
  if (!ReadParam(m, iter, &subpixel_rendering))
    return false;
  p->subpixel_rendering =
      static_cast<RendererPreferencesSubpixelRenderingEnum>(
          subpixel_rendering);

  int focus_ring_color;
  if (!ReadParam(m, iter, &focus_ring_color))
    return false;
  p->focus_ring_color = focus_ring_color;

  int thumb_active_color, thumb_inactive_color, track_color;
  int active_selection_bg_color, active_selection_fg_color;
  int inactive_selection_bg_color, inactive_selection_fg_color;
  if (!ReadParam(m, iter, &thumb_active_color) ||
      !ReadParam(m, iter, &thumb_inactive_color) ||
      !ReadParam(m, iter, &track_color) ||
      !ReadParam(m, iter, &active_selection_bg_color) ||
      !ReadParam(m, iter, &active_selection_fg_color) ||
      !ReadParam(m, iter, &inactive_selection_bg_color) ||
      !ReadParam(m, iter, &inactive_selection_fg_color))
    return false;
  p->thumb_active_color = thumb_active_color;
  p->thumb_inactive_color = thumb_inactive_color;
  p->track_color = track_color;
  p->active_selection_bg_color = active_selection_bg_color;
  p->active_selection_fg_color = active_selection_fg_color;
  p->inactive_selection_bg_color = inactive_selection_bg_color;
  p->inactive_selection_fg_color = inactive_selection_fg_color;

  if (!ReadParam(m, iter, &p->browser_handles_top_level_requests))
    return false;

  if (!ReadParam(m, iter, &p->caret_blink_interval))
    return false;

  return true;
}

void ParamTraits<RendererPreferences>::Log(const param_type& p,
                                           std::string* l) {
  l->append("<RendererPreferences>");
}

void ParamTraits<WebPreferences>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.standard_font_family);
  WriteParam(m, p.fixed_font_family);
  WriteParam(m, p.serif_font_family);
  WriteParam(m, p.sans_serif_font_family);
  WriteParam(m, p.cursive_font_family);
  WriteParam(m, p.fantasy_font_family);
  WriteParam(m, p.default_font_size);
  WriteParam(m, p.default_fixed_font_size);
  WriteParam(m, p.minimum_font_size);
  WriteParam(m, p.minimum_logical_font_size);
  WriteParam(m, p.default_encoding);
  WriteParam(m, p.javascript_enabled);
  WriteParam(m, p.web_security_enabled);
  WriteParam(m, p.javascript_can_open_windows_automatically);
  WriteParam(m, p.loads_images_automatically);
  WriteParam(m, p.plugins_enabled);
  WriteParam(m, p.dom_paste_enabled);
  WriteParam(m, p.developer_extras_enabled);
  WriteParam(m, p.inspector_settings);
  WriteParam(m, p.site_specific_quirks_enabled);
  WriteParam(m, p.shrinks_standalone_images_to_fit);
  WriteParam(m, p.uses_universal_detector);
  WriteParam(m, p.text_areas_are_resizable);
  WriteParam(m, p.java_enabled);
  WriteParam(m, p.allow_scripts_to_close_windows);
  WriteParam(m, p.uses_page_cache);
  WriteParam(m, p.remote_fonts_enabled);
  WriteParam(m, p.javascript_can_access_clipboard);
  WriteParam(m, p.xss_auditor_enabled);
  WriteParam(m, p.local_storage_enabled);
  WriteParam(m, p.databases_enabled);
  WriteParam(m, p.application_cache_enabled);
  WriteParam(m, p.tabs_to_links);
  WriteParam(m, p.user_style_sheet_enabled);
  WriteParam(m, p.user_style_sheet_location);
  WriteParam(m, p.author_and_user_styles_enabled);
  WriteParam(m, p.allow_universal_access_from_file_urls);
  WriteParam(m, p.allow_file_access_from_file_urls);
  WriteParam(m, p.experimental_webgl_enabled);
  WriteParam(m, p.show_composited_layer_borders);
  WriteParam(m, p.accelerated_compositing_enabled);
  WriteParam(m, p.accelerated_2d_canvas_enabled);
  WriteParam(m, p.memory_info_enabled);
}

bool ParamTraits<WebPreferences>::Read(const Message* m, void** iter,
                                       param_type* p) {
  return
      ReadParam(m, iter, &p->standard_font_family) &&
      ReadParam(m, iter, &p->fixed_font_family) &&
      ReadParam(m, iter, &p->serif_font_family) &&
      ReadParam(m, iter, &p->sans_serif_font_family) &&
      ReadParam(m, iter, &p->cursive_font_family) &&
      ReadParam(m, iter, &p->fantasy_font_family) &&
      ReadParam(m, iter, &p->default_font_size) &&
      ReadParam(m, iter, &p->default_fixed_font_size) &&
      ReadParam(m, iter, &p->minimum_font_size) &&
      ReadParam(m, iter, &p->minimum_logical_font_size) &&
      ReadParam(m, iter, &p->default_encoding) &&
      ReadParam(m, iter, &p->javascript_enabled) &&
      ReadParam(m, iter, &p->web_security_enabled) &&
      ReadParam(m, iter, &p->javascript_can_open_windows_automatically) &&
      ReadParam(m, iter, &p->loads_images_automatically) &&
      ReadParam(m, iter, &p->plugins_enabled) &&
      ReadParam(m, iter, &p->dom_paste_enabled) &&
      ReadParam(m, iter, &p->developer_extras_enabled) &&
      ReadParam(m, iter, &p->inspector_settings) &&
      ReadParam(m, iter, &p->site_specific_quirks_enabled) &&
      ReadParam(m, iter, &p->shrinks_standalone_images_to_fit) &&
      ReadParam(m, iter, &p->uses_universal_detector) &&
      ReadParam(m, iter, &p->text_areas_are_resizable) &&
      ReadParam(m, iter, &p->java_enabled) &&
      ReadParam(m, iter, &p->allow_scripts_to_close_windows) &&
      ReadParam(m, iter, &p->uses_page_cache) &&
      ReadParam(m, iter, &p->remote_fonts_enabled) &&
      ReadParam(m, iter, &p->javascript_can_access_clipboard) &&
      ReadParam(m, iter, &p->xss_auditor_enabled) &&
      ReadParam(m, iter, &p->local_storage_enabled) &&
      ReadParam(m, iter, &p->databases_enabled) &&
      ReadParam(m, iter, &p->application_cache_enabled) &&
      ReadParam(m, iter, &p->tabs_to_links) &&
      ReadParam(m, iter, &p->user_style_sheet_enabled) &&
      ReadParam(m, iter, &p->user_style_sheet_location) &&
      ReadParam(m, iter, &p->author_and_user_styles_enabled) &&
      ReadParam(m, iter, &p->allow_universal_access_from_file_urls) &&
      ReadParam(m, iter, &p->allow_file_access_from_file_urls) &&
      ReadParam(m, iter, &p->experimental_webgl_enabled) &&
      ReadParam(m, iter, &p->show_composited_layer_borders) &&
      ReadParam(m, iter, &p->accelerated_compositing_enabled) &&
      ReadParam(m, iter, &p->accelerated_2d_canvas_enabled) &&
      ReadParam(m, iter, &p->memory_info_enabled);
}

void ParamTraits<WebPreferences>::Log(const param_type& p, std::string* l) {
  l->append("<WebPreferences>");
}

void ParamTraits<WebDropData>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.identity);
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
      ReadParam(m, iter, &p->identity) &&
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

void ParamTraits<WebMenuItem>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.label);
  WriteParam(m, p.type);
  WriteParam(m, p.enabled);
  WriteParam(m, p.checked);
  WriteParam(m, p.action);
}

bool ParamTraits<WebMenuItem>::Read(const Message* m,
                                    void** iter,
                                    param_type* p) {
  return
      ReadParam(m, iter, &p->label) &&
      ReadParam(m, iter, &p->type) &&
      ReadParam(m, iter, &p->enabled) &&
      ReadParam(m, iter, &p->checked) &&
      ReadParam(m, iter, &p->action);
}

void ParamTraits<WebMenuItem>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.label, l);
  l->append(", ");
  LogParam(p.type, l);
  l->append(", ");
  LogParam(p.enabled, l);
  l->append(", ");
  LogParam(p.checked, l);
  l->append(", ");
  LogParam(p.action, l);
  l->append(")");
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
  return p->Parse(spec);
}

void ParamTraits<URLPattern>::Log(const param_type& p, std::string* l) {
  LogParam(p.GetAsString(), l);
}

void ParamTraits<EditCommand>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.value);
}

bool ParamTraits<EditCommand>::Read(const Message* m, void** iter,
                                    param_type* p) {
  return ReadParam(m, iter, &p->name) && ReadParam(m, iter, &p->value);
}

void ParamTraits<EditCommand>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(":");
  LogParam(p.value, l);
  l->append(")");
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

void ParamTraits<appcache::AppCacheResourceInfo>::Write(Message* m,
                                                        const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.size);
  WriteParam(m, p.is_manifest);
  WriteParam(m, p.is_master);
  WriteParam(m, p.is_fallback);
  WriteParam(m, p.is_foreign);
  WriteParam(m, p.is_explicit);
}

bool ParamTraits<appcache::AppCacheResourceInfo>::Read(
    const Message* m, void** iter, param_type* p) {
  return ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->size) &&
      ReadParam(m, iter, &p->is_manifest) &&
      ReadParam(m, iter, &p->is_master) &&
      ReadParam(m, iter, &p->is_fallback) &&
      ReadParam(m, iter, &p->is_foreign) &&
      ReadParam(m, iter, &p->is_explicit);
}

void ParamTraits<appcache::AppCacheResourceInfo>::Log(const param_type& p,
                                                      std::string* l) {
  l->append("(");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.size, l);
  l->append(", ");
  LogParam(p.is_manifest, l);
  l->append(", ");
  LogParam(p.is_master, l);
  l->append(", ");
  LogParam(p.is_fallback, l);
  l->append(", ");
  LogParam(p.is_foreign, l);
  l->append(", ");
  LogParam(p.is_explicit, l);
  l->append(")");
}

void ParamTraits<appcache::AppCacheInfo>::Write(Message* m,
                                                const param_type& p) {
  WriteParam(m, p.manifest_url);
  WriteParam(m, p.creation_time);
  WriteParam(m, p.last_update_time);
  WriteParam(m, p.last_access_time);
  WriteParam(m, p.cache_id);
  WriteParam(m, p.status);
  WriteParam(m, p.size);
  WriteParam(m, p.is_complete);
}

bool ParamTraits<appcache::AppCacheInfo>::Read(const Message* m, void** iter,
                                               param_type* p) {
  return ReadParam(m, iter, &p->manifest_url) &&
      ReadParam(m, iter, &p->creation_time) &&
      ReadParam(m, iter, &p->last_update_time) &&
      ReadParam(m, iter, &p->last_access_time) &&
      ReadParam(m, iter, &p->cache_id) &&
      ReadParam(m, iter, &p->status) &&
      ReadParam(m, iter, &p->size) &&
      ReadParam(m, iter, &p->is_complete);
}

void ParamTraits<appcache::AppCacheInfo>::Log(const param_type& p,
                                              std::string* l) {
  l->append("(");
  LogParam(p.manifest_url, l);
  l->append(", ");
  LogParam(p.creation_time, l);
  l->append(", ");
  LogParam(p.last_update_time, l);
  l->append(", ");
  LogParam(p.last_access_time, l);
  l->append(", ");
  LogParam(p.cache_id, l);
  l->append(", ");
  LogParam(p.status, l);
  l->append(", ");
  LogParam(p.size, l);
  l->append(")");
  LogParam(p.is_complete, l);
  l->append(", ");
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
  l->append(")");
}

void ParamTraits<AudioBuffersState>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.pending_bytes);
  WriteParam(m, p.hardware_delay_bytes);
  WriteParam(m, p.timestamp);
}

bool ParamTraits<AudioBuffersState>::Read(const Message* m,
                                    void** iter,
                                    param_type* p) {
  return
      ReadParam(m, iter, &p->pending_bytes) &&
      ReadParam(m, iter, &p->hardware_delay_bytes) &&
      ReadParam(m, iter, &p->timestamp);
}

void ParamTraits<AudioBuffersState>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.pending_bytes, l);
  l->append(", ");
  LogParam(p.hardware_delay_bytes, l);
  l->append(", ");
  LogParam(p.timestamp, l);
  l->append(")");
}

}  // namespace IPC
