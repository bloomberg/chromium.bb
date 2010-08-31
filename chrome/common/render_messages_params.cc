// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages_params.h"

#include "chrome/common/navigation_gesture.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/indexed_db_param_traits.h"
#include "chrome/common/render_messages.h"
#include "net/base/upload_data.h"

bool ViewMsg_Print_Params::Equals(const ViewMsg_Print_Params& rhs) const {
  return page_size == rhs.page_size &&
         printable_size == rhs.printable_size &&
         margin_top == rhs.margin_top &&
         margin_left == rhs.margin_left &&
         dpi == rhs.dpi &&
         min_shrink == rhs.min_shrink &&
         max_shrink == rhs.max_shrink &&
         desired_dpi == rhs.desired_dpi &&
         selection_only == rhs.selection_only;
}

bool ViewMsg_Print_Params::IsEmpty() const {
  return !document_cookie && !desired_dpi && !max_shrink && !min_shrink &&
         !dpi && printable_size.IsEmpty() && !selection_only &&
         page_size.IsEmpty() && !margin_top && !margin_left;
}

ViewMsg_ExecuteCode_Params::ViewMsg_ExecuteCode_Params() {
}

ViewMsg_ExecuteCode_Params::ViewMsg_ExecuteCode_Params(
    int request_id,
    const std::string& extension_id,
    const std::vector<URLPattern>& host_permissions,
    bool is_javascript,
    const std::string& code,
    bool all_frames)
    : request_id(request_id), extension_id(extension_id),
      host_permissions(host_permissions), is_javascript(is_javascript),
      code(code), all_frames(all_frames) {
}

namespace IPC {

// Self contained templates which are only used inside serializing Params
// structs.
template<>
struct ParamTraits<ViewMsg_Navigate_Params::NavigationType> {
  typedef ViewMsg_Navigate_Params::NavigationType param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<ViewMsg_Navigate_Params::NavigationType>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string event;
    switch (p) {
      case ViewMsg_Navigate_Params::RELOAD:
        event = "NavigationType_RELOAD";
        break;

      case ViewMsg_Navigate_Params::RELOAD_IGNORING_CACHE:
        event = "NavigationType_RELOAD_IGNORING_CACHE";
        break;

      case ViewMsg_Navigate_Params::RESTORE:
        event = "NavigationType_RESTORE";
        break;

      case ViewMsg_Navigate_Params::NORMAL:
        event = "NavigationType_NORMA";
        break;

      default:
        event = "NavigationType_UNKNOWN";
        break;
    }
    LogParam(event, l);
  }
};

template <>
struct ParamTraits<ResourceType::Type> {
  typedef ResourceType::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type) || !ResourceType::ValidType(type))
      return false;
    *p = ResourceType::FromInt(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string type;
    switch (p) {
      case ResourceType::MAIN_FRAME:
        type = "MAIN_FRAME";
        break;
      case ResourceType::SUB_FRAME:
        type = "SUB_FRAME";
        break;
      case ResourceType::SUB_RESOURCE:
        type = "SUB_RESOURCE";
        break;
      case ResourceType::OBJECT:
        type = "OBJECT";
        break;
      case ResourceType::MEDIA:
        type = "MEDIA";
        break;
      default:
        type = "UNKNOWN";
        break;
    }

    LogParam(type, l);
  }
};

template<>
struct ParamTraits<NavigationGesture> {
  typedef NavigationGesture param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<NavigationGesture>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string event;
    switch (p) {
      case NavigationGestureUser:
        event = "GESTURE_USER";
        break;
      case NavigationGestureAuto:
        event = "GESTURE_AUTO";
        break;
      default:
        event = "GESTURE_UNKNOWN";
        break;
    }
    LogParam(event, l);
  }
};

// Traits for AudioManager::Format.
template <>
struct ParamTraits<AudioManager::Format> {
  typedef AudioManager::Format param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AudioManager::Format>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string format;
    switch (p) {
     case AudioManager::AUDIO_PCM_LINEAR:
       format = "AUDIO_PCM_LINEAR";
       break;
     case AudioManager::AUDIO_PCM_LOW_LATENCY:
       format = "AUDIO_PCM_LOW_LATENCY";
       break;
     case AudioManager::AUDIO_MOCK:
       format = "AUDIO_MOCK";
       break;
     default:
       format = "AUDIO_LAST_FORMAT";
       break;
    }
    LogParam(format, l);
  }
};

template <>
struct ParamTraits<WindowContainerType> {
  typedef WindowContainerType param_type;
  static void Write(Message* m, const param_type& p) {
    int val = static_cast<int>(p);
    WriteParam(m, val);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int val = 0;
    if (!ReadParam(m, iter, &val) ||
        val < WINDOW_CONTAINER_TYPE_NORMAL ||
        val >= WINDOW_CONTAINER_TYPE_MAX_VALUE)
      return false;
    *p = static_cast<param_type>(val);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    ParamTraits<int>::Log(static_cast<int>(p), l);
  }
};

template <>
struct ParamTraits<Extension::Location> {
  typedef Extension::Location param_type;
  static void Write(Message* m, const param_type& p) {
    int val = static_cast<int>(p);
    WriteParam(m, val);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int val = 0;
    if (!ReadParam(m, iter, &val) ||
        val < Extension::INVALID ||
        val >= Extension::EXTERNAL_PREF_DOWNLOAD)
      return false;
    *p = static_cast<param_type>(val);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    ParamTraits<int>::Log(static_cast<int>(p), l);
  }
};


void ParamTraits<ViewMsg_Navigate_Params>::Write(Message* m,
                                                 const param_type& p) {
  WriteParam(m, p.page_id);
  WriteParam(m, p.pending_history_list_offset);
  WriteParam(m, p.current_history_list_offset);
  WriteParam(m, p.current_history_list_length);
  WriteParam(m, p.url);
  WriteParam(m, p.referrer);
  WriteParam(m, p.transition);
  WriteParam(m, p.state);
  WriteParam(m, p.navigation_type);
  WriteParam(m, p.request_time);
}

bool ParamTraits<ViewMsg_Navigate_Params>::Read(const Message* m, void** iter,
                                                param_type* p) {
  return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->pending_history_list_offset) &&
      ReadParam(m, iter, &p->current_history_list_offset) &&
      ReadParam(m, iter, &p->current_history_list_length) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->transition) &&
      ReadParam(m, iter, &p->state) &&
      ReadParam(m, iter, &p->navigation_type) &&
      ReadParam(m, iter, &p->request_time);
}

void ParamTraits<ViewMsg_Navigate_Params>::Log(const param_type& p,
                                               std::string* l) {
  l->append("(");
  LogParam(p.page_id, l);
  l->append(", ");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.transition, l);
  l->append(", ");
  LogParam(p.state, l);
  l->append(", ");
  LogParam(p.navigation_type, l);
  l->append(", ");
  LogParam(p.request_time, l);
  l->append(")");
}

void ParamTraits<ViewMsg_AudioStreamState_Params>::Write(Message* m,
                                                         const param_type& p) {
  m->WriteInt(p.state);
}

bool ParamTraits<ViewMsg_AudioStreamState_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  p->state = static_cast<ViewMsg_AudioStreamState_Params::State>(type);
  return true;
}

void ParamTraits<ViewMsg_AudioStreamState_Params>::Log(const param_type& p,
                                                       std::string* l) {
  std::string state;
  switch (p.state) {
    case ViewMsg_AudioStreamState_Params::kPlaying:
      state = "ViewMsg_AudioStreamState_Params::kPlaying";
      break;
    case ViewMsg_AudioStreamState_Params::kPaused:
      state = "ViewMsg_AudioStreamState_Params::kPaused";
      break;
    case ViewMsg_AudioStreamState_Params::kError:
      state = "ViewMsg_AudioStreamState_Params::kError";
      break;
    default:
      state = "UNKNOWN";
      break;
  }
  LogParam(state, l);
}

void ParamTraits<ViewMsg_StopFinding_Params>::Write(Message* m,
                                                    const param_type& p) {
  m->WriteInt(p.action);
}

bool ParamTraits<ViewMsg_StopFinding_Params>::Read(const Message* m,
                                                   void** iter,
                                                   param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  p->action = static_cast<ViewMsg_StopFinding_Params::Action>(type);
  return true;
}

void ParamTraits<ViewMsg_StopFinding_Params>::Log(const param_type& p,
                                                  std::string* l) {
  std::string action;
  switch (p.action) {
    case ViewMsg_StopFinding_Params::kClearSelection:
      action = "ViewMsg_StopFinding_Params::kClearSelection";
      break;
    case ViewMsg_StopFinding_Params::kKeepSelection:
      action = "ViewMsg_StopFinding_Params::kKeepSelection";
      break;
    case ViewMsg_StopFinding_Params::kActivateSelection:
      action = "ViewMsg_StopFinding_Params::kActivateSelection";
      break;
    default:
      action = "UNKNOWN";
      break;
  }
  LogParam(action, l);
}

void ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Write(
    Message* m, const param_type& p) {
  m->WriteInt(p.state);
}

bool ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Read(
    const Message* m, void** iter, param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  p->state = static_cast<param_type::State>(type);
  return true;
}

void ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Log(
    const param_type& p, std::string* l) {
  std::string state;
  switch (p.state) {
    case ViewHostMsg_GetSearchProviderInstallState_Params::DENIED:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::DENIED";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::NOT_INSTALLED:
      state =
          "ViewHostMsg_GetSearchProviderInstallState_Params::NOT_INSTALLED";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::
        INSTALLED_BUT_NOT_DEFAULT:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::"
              "INSTALLED_BUT_NOT_DEFAULT";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::
        INSTALLED_AS_DEFAULT:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::"
              "INSTALLED_AS_DEFAULT";
      break;
    default:
      state = "UNKNOWN";
      break;
  }
  LogParam(state, l);
}

void ParamTraits<ViewHostMsg_FrameNavigate_Params>::Write(Message* m,
                                                          const param_type& p) {
  WriteParam(m, p.page_id);
  WriteParam(m, p.url);
  WriteParam(m, p.referrer);
  WriteParam(m, p.transition);
  WriteParam(m, p.redirects);
  WriteParam(m, p.should_update_history);
  WriteParam(m, p.searchable_form_url);
  WriteParam(m, p.searchable_form_encoding);
  WriteParam(m, p.password_form);
  WriteParam(m, p.security_info);
  WriteParam(m, p.gesture);
  WriteParam(m, p.contents_mime_type);
  WriteParam(m, p.is_post);
  WriteParam(m, p.is_content_filtered);
  WriteParam(m, p.was_within_same_page);
  WriteParam(m, p.http_status_code);
}

bool ParamTraits<ViewHostMsg_FrameNavigate_Params>::Read(const Message* m,
                                                         void** iter,
                                                         param_type* p) {
  return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->transition) &&
      ReadParam(m, iter, &p->redirects) &&
      ReadParam(m, iter, &p->should_update_history) &&
      ReadParam(m, iter, &p->searchable_form_url) &&
      ReadParam(m, iter, &p->searchable_form_encoding) &&
      ReadParam(m, iter, &p->password_form) &&
      ReadParam(m, iter, &p->security_info) &&
      ReadParam(m, iter, &p->gesture) &&
      ReadParam(m, iter, &p->contents_mime_type) &&
      ReadParam(m, iter, &p->is_post) &&
      ReadParam(m, iter, &p->is_content_filtered) &&
      ReadParam(m, iter, &p->was_within_same_page) &&
      ReadParam(m, iter, &p->http_status_code);
}

void ParamTraits<ViewHostMsg_FrameNavigate_Params>::Log(const param_type& p,
                                                        std::string* l) {
  l->append("(");
  LogParam(p.page_id, l);
  l->append(", ");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.referrer, l);
  l->append(", ");
  LogParam(p.transition, l);
  l->append(", ");
  LogParam(p.redirects, l);
  l->append(", ");
  LogParam(p.should_update_history, l);
  l->append(", ");
  LogParam(p.searchable_form_url, l);
  l->append(", ");
  LogParam(p.searchable_form_encoding, l);
  l->append(", ");
  LogParam(p.password_form, l);
  l->append(", ");
  LogParam(p.security_info, l);
  l->append(", ");
  LogParam(p.gesture, l);
  l->append(", ");
  LogParam(p.contents_mime_type, l);
  l->append(", ");
  LogParam(p.is_post, l);
  l->append(", ");
  LogParam(p.is_content_filtered, l);
  l->append(", ");
  LogParam(p.was_within_same_page, l);
  l->append(", ");
  LogParam(p.http_status_code, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_UpdateRect_Params>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.bitmap);
  WriteParam(m, p.bitmap_rect);
  WriteParam(m, p.dx);
  WriteParam(m, p.dy);
  WriteParam(m, p.scroll_rect);
  WriteParam(m, p.copy_rects);
  WriteParam(m, p.view_size);
  WriteParam(m, p.plugin_window_moves);
  WriteParam(m, p.flags);
}

bool ParamTraits<ViewHostMsg_UpdateRect_Params>::Read(
    const Message* m, void** iter, param_type* p) {
  return
      ReadParam(m, iter, &p->bitmap) &&
      ReadParam(m, iter, &p->bitmap_rect) &&
      ReadParam(m, iter, &p->dx) &&
      ReadParam(m, iter, &p->dy) &&
      ReadParam(m, iter, &p->scroll_rect) &&
      ReadParam(m, iter, &p->copy_rects) &&
      ReadParam(m, iter, &p->view_size) &&
      ReadParam(m, iter, &p->plugin_window_moves) &&
      ReadParam(m, iter, &p->flags);
}

void ParamTraits<ViewHostMsg_UpdateRect_Params>::Log(const param_type& p,
                                                     std::string* l) {
  l->append("(");
  LogParam(p.bitmap, l);
  l->append(", ");
  LogParam(p.bitmap_rect, l);
  l->append(", ");
  LogParam(p.dx, l);
  l->append(", ");
  LogParam(p.dy, l);
  l->append(", ");
  LogParam(p.scroll_rect, l);
  l->append(", ");
  LogParam(p.copy_rects, l);
  l->append(", ");
  LogParam(p.view_size, l);
  l->append(", ");
  LogParam(p.plugin_window_moves, l);
  l->append(", ");
  LogParam(p.flags, l);
  l->append(")");
}

void ParamTraits<ViewMsg_ClosePage_Params>::Write(Message* m,
                                                  const param_type& p) {
  WriteParam(m, p.closing_process_id);
  WriteParam(m, p.closing_route_id);
  WriteParam(m, p.for_cross_site_transition);
  WriteParam(m, p.new_render_process_host_id);
  WriteParam(m, p.new_request_id);
}

bool ParamTraits<ViewMsg_ClosePage_Params>::Read(const Message* m,
                                                 void** iter,
                                                 param_type* r) {
  return ReadParam(m, iter, &r->closing_process_id) &&
      ReadParam(m, iter, &r->closing_route_id) &&
      ReadParam(m, iter, &r->for_cross_site_transition) &&
      ReadParam(m, iter, &r->new_render_process_host_id) &&
      ReadParam(m, iter, &r->new_request_id);
}

void ParamTraits<ViewMsg_ClosePage_Params>::Log(const param_type& p,
                                                std::string* l) {
  l->append("(");
  LogParam(p.closing_process_id, l);
  l->append(", ");
  LogParam(p.closing_route_id, l);
  l->append(", ");
  LogParam(p.for_cross_site_transition, l);
  l->append(", ");
  LogParam(p.new_render_process_host_id, l);
  l->append(", ");
  LogParam(p.new_request_id, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_Resource_Request>::Write(Message* m,
                                                      const param_type& p) {
  WriteParam(m, p.method);
  WriteParam(m, p.url);
  WriteParam(m, p.first_party_for_cookies);
  WriteParam(m, p.referrer);
  WriteParam(m, p.frame_origin);
  WriteParam(m, p.main_frame_origin);
  WriteParam(m, p.headers);
  WriteParam(m, p.load_flags);
  WriteParam(m, p.origin_child_id);
  WriteParam(m, p.resource_type);
  WriteParam(m, p.request_context);
  WriteParam(m, p.appcache_host_id);
  WriteParam(m, p.upload_data);
  WriteParam(m, p.download_to_file);
  WriteParam(m, p.host_renderer_id);
  WriteParam(m, p.host_render_view_id);
}

bool ParamTraits<ViewHostMsg_Resource_Request>::Read(const Message* m,
                                                     void** iter,
                                                     param_type* r) {
  return
      ReadParam(m, iter, &r->method) &&
      ReadParam(m, iter, &r->url) &&
      ReadParam(m, iter, &r->first_party_for_cookies) &&
      ReadParam(m, iter, &r->referrer) &&
      ReadParam(m, iter, &r->frame_origin) &&
      ReadParam(m, iter, &r->main_frame_origin) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->load_flags) &&
      ReadParam(m, iter, &r->origin_child_id) &&
      ReadParam(m, iter, &r->resource_type) &&
      ReadParam(m, iter, &r->request_context) &&
      ReadParam(m, iter, &r->appcache_host_id) &&
      ReadParam(m, iter, &r->upload_data) &&
      ReadParam(m, iter, &r->download_to_file) &&
      ReadParam(m, iter, &r->host_renderer_id) &&
      ReadParam(m, iter, &r->host_render_view_id);
}

void ParamTraits<ViewHostMsg_Resource_Request>::Log(const param_type& p,
                                                    std::string* l) {
  l->append("(");
  LogParam(p.method, l);
  l->append(", ");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.referrer, l);
  l->append(", ");
  LogParam(p.frame_origin, l);
  l->append(", ");
  LogParam(p.main_frame_origin, l);
  l->append(", ");
  LogParam(p.load_flags, l);
  l->append(", ");
  LogParam(p.origin_child_id, l);
  l->append(", ");
  LogParam(p.resource_type, l);
  l->append(", ");
  LogParam(p.request_context, l);
  l->append(", ");
  LogParam(p.appcache_host_id, l);
  l->append(", ");
  LogParam(p.download_to_file, l);
  l->append(", ");
  LogParam(p.host_renderer_id, l);
  l->append(", ");
  LogParam(p.host_render_view_id, l);
  l->append(")");
}

void ParamTraits<ViewMsg_Print_Params>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.page_size);
  WriteParam(m, p.printable_size);
  WriteParam(m, p.margin_top);
  WriteParam(m, p.margin_left);
  WriteParam(m, p.dpi);
  WriteParam(m, p.min_shrink);
  WriteParam(m, p.max_shrink);
  WriteParam(m, p.desired_dpi);
  WriteParam(m, p.document_cookie);
  WriteParam(m, p.selection_only);
}

bool ParamTraits<ViewMsg_Print_Params>::Read(const Message* m,
                                             void** iter,
                                             param_type* p) {
  return ReadParam(m, iter, &p->page_size) &&
      ReadParam(m, iter, &p->printable_size) &&
      ReadParam(m, iter, &p->margin_top) &&
      ReadParam(m, iter, &p->margin_left) &&
      ReadParam(m, iter, &p->dpi) &&
      ReadParam(m, iter, &p->min_shrink) &&
      ReadParam(m, iter, &p->max_shrink) &&
      ReadParam(m, iter, &p->desired_dpi) &&
      ReadParam(m, iter, &p->document_cookie) &&
      ReadParam(m, iter, &p->selection_only);
}

void ParamTraits<ViewMsg_Print_Params>::Log(const param_type& p,
                                            std::string* l) {
  l->append("<ViewMsg_Print_Params>");
}

void ParamTraits<ViewMsg_PrintPage_Params>::Write(Message* m,
                                                  const param_type& p) {
  WriteParam(m, p.params);
  WriteParam(m, p.page_number);
}

bool ParamTraits<ViewMsg_PrintPage_Params>::Read(const Message* m,
                                                 void** iter,
                                                 param_type* p) {
  return ReadParam(m, iter, &p->params) &&
      ReadParam(m, iter, &p->page_number);
}

void ParamTraits<ViewMsg_PrintPage_Params>::Log(const param_type& p,
                                                std::string* l) {
  l->append("<ViewMsg_PrintPage_Params>");
}

void ParamTraits<ViewMsg_PrintPages_Params>::Write(Message* m,
                                                   const param_type& p) {
  WriteParam(m, p.params);
  WriteParam(m, p.pages);
}

bool ParamTraits<ViewMsg_PrintPages_Params>::Read(const Message* m,
                                                  void** iter,
                                                  param_type* p) {
  return ReadParam(m, iter, &p->params) &&
      ReadParam(m, iter, &p->pages);
}

void ParamTraits<ViewMsg_PrintPages_Params>::Log(const param_type& p,
                                                 std::string* l) {
  l->append("<ViewMsg_PrintPages_Params>");
}

void ParamTraits<ViewHostMsg_DidPrintPage_Params>::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.metafile_data_handle);
  WriteParam(m, p.data_size);
  WriteParam(m, p.document_cookie);
  WriteParam(m, p.page_number);
  WriteParam(m, p.actual_shrink);
  WriteParam(m, p.page_size);
  WriteParam(m, p.content_area);
  WriteParam(m, p.has_visible_overlays);
}

bool ParamTraits<ViewHostMsg_DidPrintPage_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  return ReadParam(m, iter, &p->metafile_data_handle) &&
      ReadParam(m, iter, &p->data_size) &&
      ReadParam(m, iter, &p->document_cookie) &&
      ReadParam(m, iter, &p->page_number) &&
      ReadParam(m, iter, &p->actual_shrink) &&
      ReadParam(m, iter, &p->page_size) &&
      ReadParam(m, iter, &p->content_area) &&
      ReadParam(m, iter, &p->has_visible_overlays);
}

void ParamTraits<ViewHostMsg_DidPrintPage_Params>::Log(const param_type& p,
                                                       std::string* l) {
  l->append("<ViewHostMsg_DidPrintPage_Params>");
}

void ParamTraits<ViewHostMsg_Audio_CreateStream_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.format);
  WriteParam(m, p.channels);
  WriteParam(m, p.sample_rate);
  WriteParam(m, p.bits_per_sample);
  WriteParam(m, p.packet_size);
}

bool ParamTraits<ViewHostMsg_Audio_CreateStream_Params>::Read(const Message* m,
                                                              void** iter,
                                                              param_type* p) {
  return
      ReadParam(m, iter, &p->format) &&
      ReadParam(m, iter, &p->channels) &&
      ReadParam(m, iter, &p->sample_rate) &&
      ReadParam(m, iter, &p->bits_per_sample) &&
      ReadParam(m, iter, &p->packet_size);
}

void ParamTraits<ViewHostMsg_Audio_CreateStream_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<ViewHostMsg_Audio_CreateStream_Params>(");
  LogParam(p.format, l);
  l->append(", ");
  LogParam(p.channels, l);
  l->append(", ");
  LogParam(p.sample_rate, l);
  l->append(", ");
  LogParam(p.bits_per_sample, l);
  l->append(", ");
  LogParam(p.packet_size, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_ShowPopup_Params>::Write(Message* m,
                                                      const param_type& p) {
  WriteParam(m, p.bounds);
  WriteParam(m, p.item_height);
  WriteParam(m, p.item_font_size);
  WriteParam(m, p.selected_item);
  WriteParam(m, p.popup_items);
  WriteParam(m, p.right_aligned);
}

bool ParamTraits<ViewHostMsg_ShowPopup_Params>::Read(const Message* m,
                                                     void** iter,
                                                     param_type* p) {
  return
      ReadParam(m, iter, &p->bounds) &&
      ReadParam(m, iter, &p->item_height) &&
      ReadParam(m, iter, &p->item_font_size) &&
      ReadParam(m, iter, &p->selected_item) &&
      ReadParam(m, iter, &p->popup_items) &&
      ReadParam(m, iter, &p->right_aligned);
}

void ParamTraits<ViewHostMsg_ShowPopup_Params>::Log(const param_type& p,
                                                    std::string* l) {
  l->append("(");
  LogParam(p.bounds, l);
  l->append(", ");
  LogParam(p.item_height, l);
  l->append(", ");
  LogParam(p.item_font_size, l);
  l->append(", ");
  LogParam(p.selected_item, l);
  l->append(", ");
  LogParam(p.popup_items, l);
  l->append(", ");
  LogParam(p.right_aligned, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_ScriptedPrint_Params>::Write(Message* m,
                                                          const param_type& p) {
  WriteParam(m, p.routing_id);
  WriteParam(m, p.host_window_id);
  WriteParam(m, p.cookie);
  WriteParam(m, p.expected_pages_count);
  WriteParam(m, p.has_selection);
  WriteParam(m, p.use_overlays);
}

bool ParamTraits<ViewHostMsg_ScriptedPrint_Params>::Read(const Message* m,
                                                         void** iter,
                                                         param_type* p) {
  return
      ReadParam(m, iter, &p->routing_id) &&
      ReadParam(m, iter, &p->host_window_id) &&
      ReadParam(m, iter, &p->cookie) &&
      ReadParam(m, iter, &p->expected_pages_count) &&
      ReadParam(m, iter, &p->has_selection) &&
      ReadParam(m, iter, &p->use_overlays);
}

void ParamTraits<ViewHostMsg_ScriptedPrint_Params>::Log(const param_type& p,
                                                        std::string* l) {
  l->append("(");
  LogParam(p.routing_id, l);
  l->append(", ");
  LogParam(p.host_window_id, l);
  l->append(", ");
  LogParam(p.cookie, l);
  l->append(", ");
  LogParam(p.expected_pages_count, l);
  l->append(", ");
  LogParam(p.has_selection, l);
  l->append(",");
  LogParam(p.use_overlays, l);
  l->append(")");
}

void ParamTraits<ViewMsg_DOMStorageEvent_Params>::Write(Message* m,
                                                        const param_type& p) {
  WriteParam(m, p.key_);
  WriteParam(m, p.old_value_);
  WriteParam(m, p.new_value_);
  WriteParam(m, p.origin_);
  WriteParam(m, p.url_);
  WriteParam(m, p.storage_type_);
}

bool ParamTraits<ViewMsg_DOMStorageEvent_Params>::Read(const Message* m,
                                                       void** iter,
                                                       param_type* p) {
  return
      ReadParam(m, iter, &p->key_) &&
      ReadParam(m, iter, &p->old_value_) &&
      ReadParam(m, iter, &p->new_value_) &&
      ReadParam(m, iter, &p->origin_) &&
      ReadParam(m, iter, &p->url_) &&
      ReadParam(m, iter, &p->storage_type_);
}

void ParamTraits<ViewMsg_DOMStorageEvent_Params>::Log(const param_type& p,
                                                      std::string* l) {
  l->append("(");
  LogParam(p.key_, l);
  l->append(", ");
  LogParam(p.old_value_, l);
  l->append(", ");
  LogParam(p.new_value_, l);
  l->append(", ");
  LogParam(p.origin_, l);
  l->append(", ");
  LogParam(p.url_, l);
  l->append(", ");
  LogParam(p.storage_type_, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_IDBFactoryOpen_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.routing_id_);
  WriteParam(m, p.response_id_);
  WriteParam(m, p.origin_);
  WriteParam(m, p.name_);
  WriteParam(m, p.description_);
}

bool ParamTraits<ViewHostMsg_IDBFactoryOpen_Params>::Read(const Message* m,
                                                          void** iter,
                                                          param_type* p) {
  return
      ReadParam(m, iter, &p->routing_id_) &&
      ReadParam(m, iter, &p->response_id_) &&
      ReadParam(m, iter, &p->origin_) &&
      ReadParam(m, iter, &p->name_) &&
      ReadParam(m, iter, &p->description_);
}

void ParamTraits<ViewHostMsg_IDBFactoryOpen_Params>::Log(const param_type& p,
                                                         std::string* l) {
  l->append("(");
  LogParam(p.routing_id_, l);
  l->append(", ");
  LogParam(p.response_id_, l);
  l->append(", ");
  LogParam(p.origin_, l);
  l->append(", ");
  LogParam(p.name_, l);
  l->append(", ");
  LogParam(p.description_, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_IDBDatabaseCreateObjectStore_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.response_id_);
  WriteParam(m, p.name_);
  WriteParam(m, p.key_path_);
  WriteParam(m, p.auto_increment_);
  WriteParam(m, p.idb_database_id_);
}

bool ParamTraits<ViewHostMsg_IDBDatabaseCreateObjectStore_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->response_id_) &&
      ReadParam(m, iter, &p->name_) &&
      ReadParam(m, iter, &p->key_path_) &&
      ReadParam(m, iter, &p->auto_increment_) &&
      ReadParam(m, iter, &p->idb_database_id_);
}

void ParamTraits<ViewHostMsg_IDBDatabaseCreateObjectStore_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.response_id_, l);
  l->append(", ");
  LogParam(p.name_, l);
  l->append(", ");
  LogParam(p.key_path_, l);
  l->append(", ");
  LogParam(p.auto_increment_, l);
  l->append(", ");
  LogParam(p.idb_database_id_, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_IDBObjectStoreCreateIndex_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.response_id_);
  WriteParam(m, p.name_);
  WriteParam(m, p.key_path_);
  WriteParam(m, p.unique_);
  WriteParam(m, p.idb_object_store_id_);
}

bool ParamTraits<ViewHostMsg_IDBObjectStoreCreateIndex_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->response_id_) &&
      ReadParam(m, iter, &p->name_) &&
      ReadParam(m, iter, &p->key_path_) &&
      ReadParam(m, iter, &p->unique_) &&
      ReadParam(m, iter, &p->idb_object_store_id_);
}

void ParamTraits<ViewHostMsg_IDBObjectStoreCreateIndex_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.response_id_, l);
  l->append(", ");
  LogParam(p.name_, l);
  l->append(", ");
  LogParam(p.key_path_, l);
  l->append(", ");
  LogParam(p.unique_, l);
  l->append(", ");
  LogParam(p.idb_object_store_id_, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_IDBObjectStoreOpenCursor_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.response_id_);
  WriteParam(m, p.left_key_);
  WriteParam(m, p.right_key_);
  WriteParam(m, p.flags_);
  WriteParam(m, p.direction_);
  WriteParam(m, p.idb_object_store_id_);
}

bool ParamTraits<ViewHostMsg_IDBObjectStoreOpenCursor_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->response_id_) &&
      ReadParam(m, iter, &p->left_key_) &&
      ReadParam(m, iter, &p->right_key_) &&
      ReadParam(m, iter, &p->flags_) &&
      ReadParam(m, iter, &p->direction_) &&
      ReadParam(m, iter, &p->idb_object_store_id_);
}

void ParamTraits<ViewHostMsg_IDBObjectStoreOpenCursor_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.response_id_, l);
  l->append(", ");
  LogParam(p.left_key_, l);
  l->append(", ");
  LogParam(p.right_key_, l);
  l->append(", ");
  LogParam(p.flags_, l);
  l->append(", ");
  LogParam(p.direction_, l);
  l->append(", ");
  LogParam(p.idb_object_store_id_, l);
  l->append(")");
}

void ParamTraits<ViewMsg_ExecuteCode_Params>::Write(Message* m,
                                                    const param_type& p) {
  WriteParam(m, p.request_id);
  WriteParam(m, p.extension_id);
  WriteParam(m, p.host_permissions);
  WriteParam(m, p.is_javascript);
  WriteParam(m, p.code);
  WriteParam(m, p.all_frames);
}

bool ParamTraits<ViewMsg_ExecuteCode_Params>::Read(const Message* m,
                                                   void** iter,
                                                   param_type* p) {
  return
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->extension_id) &&
      ReadParam(m, iter, &p->host_permissions) &&
      ReadParam(m, iter, &p->is_javascript) &&
      ReadParam(m, iter, &p->code) &&
      ReadParam(m, iter, &p->all_frames);
}

void ParamTraits<ViewMsg_ExecuteCode_Params>::Log(const param_type& p,
                                                  std::string* l) {
  l->append("<ViewMsg_ExecuteCode_Params>");
}

void ParamTraits<ViewHostMsg_CreateWorker_Params>::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.is_shared);
  WriteParam(m, p.name);
  WriteParam(m, p.document_id);
  WriteParam(m, p.render_view_route_id);
  WriteParam(m, p.route_id);
  WriteParam(m, p.parent_appcache_host_id);
  WriteParam(m, p.script_resource_appcache_id);
}

bool ParamTraits<ViewHostMsg_CreateWorker_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  return
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->is_shared) &&
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->document_id) &&
      ReadParam(m, iter, &p->render_view_route_id) &&
      ReadParam(m, iter, &p->route_id) &&
      ReadParam(m, iter, &p->parent_appcache_host_id) &&
      ReadParam(m, iter, &p->script_resource_appcache_id);
}

void ParamTraits<ViewHostMsg_CreateWorker_Params>::Log(const param_type& p,
                                                       std::string* l) {
  l->append("(");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.is_shared, l);
  l->append(", ");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.document_id, l);
  l->append(", ");
  LogParam(p.render_view_route_id, l);
  l->append(",");
  LogParam(p.route_id, l);
  l->append(", ");
  LogParam(p.parent_appcache_host_id, l);
  l->append(",");
  LogParam(p.script_resource_appcache_id, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_ShowNotification_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.origin);
  WriteParam(m, p.is_html);
  WriteParam(m, p.contents_url);
  WriteParam(m, p.icon_url);
  WriteParam(m, p.title);
  WriteParam(m, p.body);
  WriteParam(m, p.direction);
  WriteParam(m, p.replace_id);
  WriteParam(m, p.notification_id);
}

bool ParamTraits<ViewHostMsg_ShowNotification_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->is_html) &&
      ReadParam(m, iter, &p->contents_url) &&
      ReadParam(m, iter, &p->icon_url) &&
      ReadParam(m, iter, &p->title) &&
      ReadParam(m, iter, &p->body) &&
      ReadParam(m, iter, &p->direction) &&
      ReadParam(m, iter, &p->replace_id) &&
      ReadParam(m, iter, &p->notification_id);
}

void ParamTraits<ViewHostMsg_ShowNotification_Params>::Log(
    const param_type &p,
    std::string* l) {
  l->append("(");
  LogParam(p.origin, l);
  l->append(", ");
  LogParam(p.is_html, l);
  l->append(", ");
  LogParam(p.contents_url, l);
  l->append(", ");
  LogParam(p.icon_url, l);
  l->append(", ");
  LogParam(p.title, l);
  l->append(",");
  LogParam(p.body, l);
  l->append(",");
  LogParam(p.direction, l);
  l->append(",");
  LogParam(p.replace_id, l);
  l->append(",");
  LogParam(p.notification_id, l);
  l->append(")");
}

void ParamTraits<ViewMsg_New_Params>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.parent_window);
  WriteParam(m, p.renderer_preferences);
  WriteParam(m, p.web_preferences);
  WriteParam(m, p.view_id);
  WriteParam(m, p.session_storage_namespace_id);
  WriteParam(m, p.frame_name);
}

bool ParamTraits<ViewMsg_New_Params>::Read(const Message* m,
                                           void** iter,
                                           param_type* p) {
  return
      ReadParam(m, iter, &p->parent_window) &&
      ReadParam(m, iter, &p->renderer_preferences) &&
      ReadParam(m, iter, &p->web_preferences) &&
      ReadParam(m, iter, &p->view_id) &&
      ReadParam(m, iter, &p->session_storage_namespace_id) &&
      ReadParam(m, iter, &p->frame_name);
}

void ParamTraits<ViewMsg_New_Params>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.parent_window, l);
  l->append(", ");
  LogParam(p.renderer_preferences, l);
  l->append(", ");
  LogParam(p.web_preferences, l);
  l->append(", ");
  LogParam(p.view_id, l);
  l->append(", ");
  LogParam(p.session_storage_namespace_id, l);
  l->append(", ");
  LogParam(p.frame_name, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_CreateWindow_Params>::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.opener_id);
  WriteParam(m, p.user_gesture);
  WriteParam(m, p.window_container_type);
  WriteParam(m, p.session_storage_namespace_id);
  WriteParam(m, p.frame_name);
}

bool ParamTraits<ViewHostMsg_CreateWindow_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  return
      ReadParam(m, iter, &p->opener_id) &&
      ReadParam(m, iter, &p->user_gesture) &&
      ReadParam(m, iter, &p->window_container_type) &&
      ReadParam(m, iter, &p->session_storage_namespace_id) &&
      ReadParam(m, iter, &p->frame_name);
}

void ParamTraits<ViewHostMsg_CreateWindow_Params>::Log(const param_type& p,
                                                       std::string* l) {
  l->append("(");
  LogParam(p.opener_id, l);
  l->append(", ");
  LogParam(p.user_gesture, l);
  l->append(", ");
  LogParam(p.window_container_type, l);
  l->append(", ");
  LogParam(p.session_storage_namespace_id, l);
  l->append(", ");
  LogParam(p.frame_name, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_RunFileChooser_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, static_cast<int>(p.mode));
  WriteParam(m, p.title);
  WriteParam(m, p.default_file_name);
}

bool ParamTraits<ViewHostMsg_RunFileChooser_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  int mode;
  if (!ReadParam(m, iter, &mode))
    return false;
  if (mode != param_type::Open &&
      mode != param_type::OpenMultiple &&
      mode != param_type::OpenFolder &&
      mode != param_type::Save)
    return false;
  p->mode = static_cast<param_type::Mode>(mode);
  return
      ReadParam(m, iter, &p->title) &&
      ReadParam(m, iter, &p->default_file_name);
};

void ParamTraits<ViewHostMsg_RunFileChooser_Params>::Log(
    const param_type& p,
    std::string* l) {
  switch (p.mode) {
    case param_type::Open:
      l->append("(Open, ");
      break;
    case param_type::OpenMultiple:
      l->append("(OpenMultiple, ");
      break;
    case param_type::OpenFolder:
      l->append("(OpenFolder, ");
      break;
    case param_type::Save:
      l->append("(Save, ");
      break;
    default:
      l->append("(UNKNOWN, ");
  }
  LogParam(p.title, l);
  l->append(", ");
  LogParam(p.default_file_name, l);
}

void ParamTraits<ViewMsg_ExtensionRendererInfo>::Write(Message* m,
                                                       const param_type& p) {
  WriteParam(m, p.id);
  WriteParam(m, p.web_extent);
  WriteParam(m, p.name);
  WriteParam(m, p.icon_url);
  WriteParam(m, p.location);
}

bool ParamTraits<ViewMsg_ExtensionRendererInfo>::Read(const Message* m,
                                                      void** iter,
                                                      param_type* p) {
  return ReadParam(m, iter, &p->id) &&
      ReadParam(m, iter, &p->web_extent) &&
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->icon_url) &&
      ReadParam(m, iter, &p->location);
}

void ParamTraits<ViewMsg_ExtensionRendererInfo>::Log(const param_type& p,
                                                     std::string* l) {
  LogParam(p.id, l);
}

void ParamTraits<ViewMsg_ExtensionsUpdated_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.extensions);
}

bool ParamTraits<ViewMsg_ExtensionsUpdated_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return ReadParam(m, iter, &p->extensions);
}

void ParamTraits<ViewMsg_ExtensionsUpdated_Params>::Log(
    const param_type& p,
    std::string* l) {
  LogParam(p.extensions, l);
}

void ParamTraits<ViewMsg_DeviceOrientationUpdated_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.can_provide_alpha);
  WriteParam(m, p.alpha);
  WriteParam(m, p.can_provide_beta);
  WriteParam(m, p.beta);
  WriteParam(m, p.can_provide_gamma);
  WriteParam(m, p.gamma);
}

bool ParamTraits<ViewMsg_DeviceOrientationUpdated_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->can_provide_alpha) &&
      ReadParam(m, iter, &p->alpha) &&
      ReadParam(m, iter, &p->can_provide_beta) &&
      ReadParam(m, iter, &p->beta) &&
      ReadParam(m, iter, &p->can_provide_gamma) &&
      ReadParam(m, iter, &p->gamma);
}

void ParamTraits<ViewMsg_DeviceOrientationUpdated_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.can_provide_alpha, l);
  l->append(", ");
  LogParam(p.alpha, l);
  l->append(", ");
  LogParam(p.can_provide_beta, l);
  l->append(", ");
  LogParam(p.beta, l);
  l->append(", ");
  LogParam(p.can_provide_gamma, l);
  l->append(", ");
  LogParam(p.gamma, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_DomMessage_Params>::Write(Message* m,
                                                       const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.arguments);
  WriteParam(m, p.source_url);
  WriteParam(m, p.request_id);
  WriteParam(m, p.has_callback);
  WriteParam(m, p.user_gesture);
}

bool ParamTraits<ViewHostMsg_DomMessage_Params>::Read(const Message* m,
                                                      void** iter,
                                                      param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->arguments) &&
      ReadParam(m, iter, &p->source_url) &&
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->has_callback) &&
      ReadParam(m, iter, &p->user_gesture);
}

void ParamTraits<ViewHostMsg_DomMessage_Params>::Log(const param_type& p,
                                                     std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.arguments, l);
  l->append(", ");
  LogParam(p.source_url, l);
  l->append(", ");
  LogParam(p.request_id, l);
  l->append(", ");
  LogParam(p.has_callback, l);
  l->append(", ");
  LogParam(p.user_gesture, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_OpenFileSystemRequest_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.routing_id);
  WriteParam(m, p.request_id);
  WriteParam(m, p.origin_url);
  WriteParam(m, p.type);
  WriteParam(m, p.requested_size);
}

bool ParamTraits<ViewHostMsg_OpenFileSystemRequest_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->routing_id) &&
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->origin_url) &&
      ReadParam(m, iter, &p->type) &&
      ReadParam(m, iter, &p->requested_size);
}

void ParamTraits<ViewHostMsg_OpenFileSystemRequest_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.routing_id, l);
  l->append(", ");
  LogParam(p.request_id, l);
  l->append(", ");
  LogParam(p.origin_url, l);
  l->append(", ");
  LogParam(p.type, l);
  l->append(", ");
  LogParam(p.requested_size, l);
  l->append(")");
}

void ParamTraits<ViewMsg_FileSystem_DidReadDirectory_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.request_id);
  WriteParam(m, p.entries);
  WriteParam(m, p.has_more);
}

bool ParamTraits<ViewMsg_FileSystem_DidReadDirectory_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->entries) &&
      ReadParam(m, iter, &p->has_more);
}

void ParamTraits<ViewMsg_FileSystem_DidReadDirectory_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.request_id, l);
  l->append(", ");
  LogParam(p.entries, l);
  l->append(", ");
  LogParam(p.has_more, l);
  l->append(")");
}

void ParamTraits<ViewMsg_FileSystem_DidReadDirectory_Params::Entry>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.is_directory);
}

bool ParamTraits<ViewMsg_FileSystem_DidReadDirectory_Params::Entry>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->is_directory);
}

void ParamTraits<ViewMsg_FileSystem_DidReadDirectory_Params::Entry>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.is_directory, l);
  l->append(")");
}

}  // namespace IPC
