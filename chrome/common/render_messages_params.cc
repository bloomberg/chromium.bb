// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages_params.h"

#include "chrome/common/navigation_gesture.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/render_messages.h"
#include "net/base/upload_data.h"

ViewMsg_Navigate_Params::ViewMsg_Navigate_Params()
    : page_id(-1),
      pending_history_list_offset(-1),
      current_history_list_offset(-1),
      current_history_list_length(0),
      transition(PageTransition::LINK),
      navigation_type(NORMAL) {
}

ViewMsg_Navigate_Params::~ViewMsg_Navigate_Params() {
}

ViewHostMsg_FrameNavigate_Params::ViewHostMsg_FrameNavigate_Params()
    : page_id(0),
      frame_id(0),
      transition(PageTransition::TYPED),
      should_update_history(false),
      gesture(NavigationGestureUser),
      is_post(false),
      is_content_filtered(false),
      was_within_same_page(false),
      http_status_code(0) {
}

ViewHostMsg_FrameNavigate_Params::~ViewHostMsg_FrameNavigate_Params() {
}

ViewHostMsg_UpdateRect_Params::ViewHostMsg_UpdateRect_Params()
    : dx(0),
      dy(0),
      flags(0) {
    // On windows, bitmap is of type "struct HandleAndSequenceNum"
    memset(&bitmap, 0, sizeof(bitmap));
}

ViewHostMsg_UpdateRect_Params::~ViewHostMsg_UpdateRect_Params() {
}

ViewMsg_ClosePage_Params::ViewMsg_ClosePage_Params()
    : closing_process_id(0),
      closing_route_id(0),
      for_cross_site_transition(false),
      new_render_process_host_id(0),
      new_request_id(0) {
}

ViewMsg_ClosePage_Params::~ViewMsg_ClosePage_Params() {
}

ViewHostMsg_Resource_Request::ViewHostMsg_Resource_Request()
    : load_flags(0),
      origin_pid(0),
      resource_type(ResourceType::MAIN_FRAME),
      request_context(0),
      appcache_host_id(0),
      download_to_file(false),
      has_user_gesture(false),
      host_renderer_id(0),
      host_render_view_id(0) {
}

ViewHostMsg_Resource_Request::~ViewHostMsg_Resource_Request() {
}

ViewMsg_Print_Params::ViewMsg_Print_Params()
    : margin_top(0),
      margin_left(0),
      dpi(0),
      min_shrink(0),
      max_shrink(0),
      desired_dpi(0),
      document_cookie(0),
      selection_only(false),
      supports_alpha_blend(true) {
}

ViewMsg_Print_Params::~ViewMsg_Print_Params() {
}

bool ViewMsg_Print_Params::Equals(const ViewMsg_Print_Params& rhs) const {
  return page_size == rhs.page_size &&
         printable_size == rhs.printable_size &&
         margin_top == rhs.margin_top &&
         margin_left == rhs.margin_left &&
         dpi == rhs.dpi &&
         min_shrink == rhs.min_shrink &&
         max_shrink == rhs.max_shrink &&
         desired_dpi == rhs.desired_dpi &&
         selection_only == rhs.selection_only &&
         supports_alpha_blend == rhs.supports_alpha_blend;
}

bool ViewMsg_Print_Params::IsEmpty() const {
  return !document_cookie && !desired_dpi && !max_shrink && !min_shrink &&
         !dpi && printable_size.IsEmpty() && !selection_only &&
         page_size.IsEmpty() && !margin_top && !margin_left &&
         !supports_alpha_blend;
}

ViewMsg_PrintPage_Params::ViewMsg_PrintPage_Params()
    : page_number(0) {
}

ViewMsg_PrintPage_Params::~ViewMsg_PrintPage_Params() {
}

ViewMsg_PrintPages_Params::ViewMsg_PrintPages_Params() {
}

ViewMsg_PrintPages_Params::~ViewMsg_PrintPages_Params() {
}

ViewHostMsg_DidPreviewDocument_Params::ViewHostMsg_DidPreviewDocument_Params()
    : data_size(0), expected_pages_count(0) {
#if defined(OS_WIN)
  // Initialize |metafile_data_handle| only on Windows because it maps
  // base::SharedMemoryHandle to HANDLE. We do not need to initialize this
  // variable on Posix because it maps base::SharedMemoryHandle to
  // FileDescriptior, which has the default constructor.
  metafile_data_handle = INVALID_HANDLE_VALUE;
#endif
}

ViewHostMsg_DidPreviewDocument_Params::
    ~ViewHostMsg_DidPreviewDocument_Params() {
}

ViewHostMsg_DidPrintPage_Params::ViewHostMsg_DidPrintPage_Params()
    : data_size(0),
      document_cookie(0),
      page_number(0),
      actual_shrink(0),
      has_visible_overlays(false) {
#if defined(OS_WIN)
    // Initialize |metafile_data_handle| only on Windows because it maps
  // base::SharedMemoryHandle to HANDLE. We do not need to initialize this
  // variable on Posix because it maps base::SharedMemoryHandle to
  // FileDescriptior, which has the default constructor.
  metafile_data_handle = INVALID_HANDLE_VALUE;
#endif
}

ViewHostMsg_DidPrintPage_Params::~ViewHostMsg_DidPrintPage_Params() {
}

ViewHostMsg_Audio_CreateStream_Params::ViewHostMsg_Audio_CreateStream_Params() {
}

ViewHostMsg_Audio_CreateStream_Params::
    ~ViewHostMsg_Audio_CreateStream_Params() {
}

ViewHostMsg_ShowPopup_Params::ViewHostMsg_ShowPopup_Params()
    : item_height(0),
      item_font_size(0),
      selected_item(0),
      right_aligned(false) {
}

ViewHostMsg_ShowPopup_Params::~ViewHostMsg_ShowPopup_Params() {
}

ViewHostMsg_ScriptedPrint_Params::ViewHostMsg_ScriptedPrint_Params()
    : routing_id(0),
      host_window_id(0),
      cookie(0),
      expected_pages_count(0),
      has_selection(false),
      use_overlays(false) {
}

ViewHostMsg_ScriptedPrint_Params::~ViewHostMsg_ScriptedPrint_Params() {
}

ViewMsg_ExecuteCode_Params::ViewMsg_ExecuteCode_Params() {
}

ViewMsg_ExecuteCode_Params::ViewMsg_ExecuteCode_Params(
    int request_id,
    const std::string& extension_id,
    bool is_javascript,
    const std::string& code,
    bool all_frames)
    : request_id(request_id), extension_id(extension_id),
      is_javascript(is_javascript),
      code(code), all_frames(all_frames) {
}

ViewMsg_ExecuteCode_Params::~ViewMsg_ExecuteCode_Params() {
}

ViewHostMsg_CreateWorker_Params::ViewHostMsg_CreateWorker_Params()
    : is_shared(false),
      document_id(0),
      render_view_route_id(0),
      route_id(0),
      parent_appcache_host_id(0),
      script_resource_appcache_id(0) {
}

ViewHostMsg_CreateWorker_Params::~ViewHostMsg_CreateWorker_Params() {
}

ViewHostMsg_ShowNotification_Params::ViewHostMsg_ShowNotification_Params()
    : is_html(false),
      direction(WebKit::WebTextDirectionDefault),
      notification_id(0) {
}

ViewHostMsg_ShowNotification_Params::~ViewHostMsg_ShowNotification_Params() {
}

ViewMsg_New_Params::ViewMsg_New_Params()
    : parent_window(0),
      view_id(0),
      session_storage_namespace_id(0) {
}

ViewMsg_New_Params::~ViewMsg_New_Params() {
}

ViewHostMsg_CreateWindow_Params::ViewHostMsg_CreateWindow_Params()
    : opener_id(0),
      user_gesture(false),
      window_container_type(WINDOW_CONTAINER_TYPE_NORMAL),
      session_storage_namespace_id(0),
      opener_frame_id(0) {
}

ViewHostMsg_CreateWindow_Params::~ViewHostMsg_CreateWindow_Params() {
}

ViewHostMsg_RunFileChooser_Params::ViewHostMsg_RunFileChooser_Params()
    : mode(Open) {
}

ViewHostMsg_RunFileChooser_Params::~ViewHostMsg_RunFileChooser_Params() {
}

ViewMsg_DeviceOrientationUpdated_Params::
    ViewMsg_DeviceOrientationUpdated_Params()
    : can_provide_alpha(false),
      alpha(0),
      can_provide_beta(false),
      beta(0),
      can_provide_gamma(false),
      gamma(0) {
}

ViewMsg_DeviceOrientationUpdated_Params::
    ~ViewMsg_DeviceOrientationUpdated_Params() {
}

ViewHostMsg_DomMessage_Params::ViewHostMsg_DomMessage_Params()
    : request_id(0),
      has_callback(false),
      user_gesture(false) {
}

ViewHostMsg_DomMessage_Params::~ViewHostMsg_DomMessage_Params() {
}

ViewHostMsg_MalwareDOMDetails_Node::ViewHostMsg_MalwareDOMDetails_Node() {
}

ViewHostMsg_MalwareDOMDetails_Node::~ViewHostMsg_MalwareDOMDetails_Node() {
}

ViewHostMsg_MalwareDOMDetails_Params::ViewHostMsg_MalwareDOMDetails_Params() {
}

ViewHostMsg_MalwareDOMDetails_Params::~ViewHostMsg_MalwareDOMDetails_Params() {
}

ViewMsg_ExtensionLoaded_Params::ViewMsg_ExtensionLoaded_Params() {
}

ViewMsg_ExtensionLoaded_Params::~ViewMsg_ExtensionLoaded_Params() {
}

ViewMsg_ExtensionLoaded_Params::ViewMsg_ExtensionLoaded_Params(
    const ViewMsg_ExtensionLoaded_Params& other)
    : manifest(other.manifest->DeepCopy()),
      location(other.location),
      path(other.path),
      id(other.id) {
}

ViewMsg_ExtensionLoaded_Params::ViewMsg_ExtensionLoaded_Params(
    const Extension* extension)
    : manifest(new DictionaryValue()),
      location(extension->location()),
      path(extension->path()),
      id(extension->id()) {
  // As we need more bits of extension data in the renderer, add more keys to
  // this list.
  const char* kRendererExtensionKeys[] = {
    extension_manifest_keys::kPublicKey,
    extension_manifest_keys::kName,
    extension_manifest_keys::kVersion,
    extension_manifest_keys::kIcons,
    extension_manifest_keys::kPermissions,
    extension_manifest_keys::kApp
  };

  // Copy only the data we need.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRendererExtensionKeys); ++i) {
    Value* temp = NULL;
    if (extension->manifest_value()->Get(kRendererExtensionKeys[i], &temp))
      manifest->Set(kRendererExtensionKeys[i], temp->DeepCopy());
  }
}

scoped_refptr<Extension>
    ViewMsg_ExtensionLoaded_Params::ConvertToExtension() const {
  // Extensions that are loaded unpacked won't have a key.
  const bool kRequireKey = false;
  std::string error;

  scoped_refptr<Extension> extension(
      Extension::Create(path, location, *manifest, kRequireKey, &error));
  if (!extension.get())
    LOG(ERROR) << "Error deserializing extension: " << error;

  return extension;
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

      case ViewMsg_Navigate_Params::PRERENDER:
        event = "NavigationType_PRERENDER";
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
struct ParamTraits<AudioParameters::Format> {
  typedef AudioParameters::Format param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AudioParameters::Format>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string format;
    switch (p) {
     case AudioParameters::AUDIO_PCM_LINEAR:
       format = "AUDIO_PCM_LINEAR";
       break;
     case AudioParameters::AUDIO_PCM_LOW_LATENCY:
       format = "AUDIO_PCM_LOW_LATENCY";
       break;
     case AudioParameters::AUDIO_MOCK:
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
        val >= Extension::NUM_LOCATIONS)
      return false;
    *p = static_cast<param_type>(val);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    ParamTraits<int>::Log(static_cast<int>(p), l);
  }
};

template <>
struct ParamTraits
    <ViewHostMsg_AccessibilityNotification_Params::NotificationType> {
  typedef ViewHostMsg_AccessibilityNotification_Params params;
  typedef params::NotificationType param_type;
  static void Write(Message* m, const param_type& p) {
    int val = static_cast<int>(p);
    WriteParam(m, val);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int val = 0;
    if (!ReadParam(m, iter, &val) ||
        val < params::NOTIFICATION_TYPE_CHECK_STATE_CHANGED ||
        val > params::NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED) {
      return false;
    }
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
  WriteParam(m, p.extra_headers);
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
      ReadParam(m, iter, &p->request_time) &&
      ReadParam(m, iter, &p->extra_headers);
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
  l->append(", ");
  LogParam(p.extra_headers, l);
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

void ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Write(Message* m,
                                                      const param_type& p) {
  m->WriteInt(p.type);
}

bool ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Read(const Message* m,
                                                     void** iter,
                                                     param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  p->type = static_cast<param_type::Type>(type);
  return true;
}

void ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Log(const param_type& p,
                                                    std::string* l) {
  std::string type;
  switch (p.type) {
    case ViewHostMsg_PageHasOSDD_Type::AUTODETECTED_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::AUTODETECTED_PROVIDER";
      break;
    case ViewHostMsg_PageHasOSDD_Type::EXPLICIT_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::EXPLICIT_PROVIDER";
      break;
    case ViewHostMsg_PageHasOSDD_Type::EXPLICIT_DEFAULT_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::EXPLICIT_DEFAULT_PROVIDER";
      break;
    default:
      type = "UNKNOWN";
      break;
  }
  LogParam(type, l);
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
  WriteParam(m, p.frame_id);
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
  WriteParam(m, p.socket_address);
}

bool ParamTraits<ViewHostMsg_FrameNavigate_Params>::Read(const Message* m,
                                                         void** iter,
                                                         param_type* p) {
  return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->frame_id) &&
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
      ReadParam(m, iter, &p->http_status_code) &&
      ReadParam(m, iter, &p->socket_address);
}

void ParamTraits<ViewHostMsg_FrameNavigate_Params>::Log(const param_type& p,
                                                        std::string* l) {
  l->append("(");
  LogParam(p.page_id, l);
  l->append(", ");
  LogParam(p.frame_id, l);
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
  l->append(", ");
  LogParam(p.socket_address, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_UpdateRect_Params>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.bitmap);
  WriteParam(m, p.bitmap_rect);
  WriteParam(m, p.dx);
  WriteParam(m, p.dy);
  WriteParam(m, p.scroll_rect);
  WriteParam(m, p.scroll_offset);
  WriteParam(m, p.copy_rects);
  WriteParam(m, p.view_size);
  WriteParam(m, p.resizer_rect);
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
      ReadParam(m, iter, &p->scroll_offset) &&
      ReadParam(m, iter, &p->copy_rects) &&
      ReadParam(m, iter, &p->view_size) &&
      ReadParam(m, iter, &p->resizer_rect) &&
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
  LogParam(p.resizer_rect, l);
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
  WriteParam(m, p.headers);
  WriteParam(m, p.load_flags);
  WriteParam(m, p.origin_pid);
  WriteParam(m, p.resource_type);
  WriteParam(m, p.request_context);
  WriteParam(m, p.appcache_host_id);
  WriteParam(m, p.upload_data);
  WriteParam(m, p.download_to_file);
  WriteParam(m, p.has_user_gesture);
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
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->load_flags) &&
      ReadParam(m, iter, &r->origin_pid) &&
      ReadParam(m, iter, &r->resource_type) &&
      ReadParam(m, iter, &r->request_context) &&
      ReadParam(m, iter, &r->appcache_host_id) &&
      ReadParam(m, iter, &r->upload_data) &&
      ReadParam(m, iter, &r->download_to_file) &&
      ReadParam(m, iter, &r->has_user_gesture) &&
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
  LogParam(p.load_flags, l);
  l->append(", ");
  LogParam(p.origin_pid, l);
  l->append(", ");
  LogParam(p.resource_type, l);
  l->append(", ");
  LogParam(p.request_context, l);
  l->append(", ");
  LogParam(p.appcache_host_id, l);
  l->append(", ");
  LogParam(p.download_to_file, l);
  l->append(", ");
  LogParam(p.has_user_gesture, l);
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
  WriteParam(m, p.supports_alpha_blend);
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
      ReadParam(m, iter, &p->selection_only) &&
      ReadParam(m, iter, &p->supports_alpha_blend);
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

void ParamTraits<ViewHostMsg_DidPreviewDocument_Params>::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.metafile_data_handle);
  WriteParam(m, p.data_size);
  WriteParam(m, p.document_cookie);
  WriteParam(m, p.expected_pages_count);
}

bool ParamTraits<ViewHostMsg_DidPreviewDocument_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  return ReadParam(m, iter, &p->metafile_data_handle) &&
      ReadParam(m, iter, &p->data_size) &&
      ReadParam(m, iter, &p->document_cookie) &&
      ReadParam(m, iter, &p->expected_pages_count);
}

void ParamTraits<ViewHostMsg_DidPreviewDocument_Params>::Log(
    const param_type& p, std::string* l) {
  l->append("<ViewHostMsg_DidPreviewDocument_Params>");
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
  WriteParam(m, p.params.format);
  WriteParam(m, p.params.channels);
  WriteParam(m, p.params.sample_rate);
  WriteParam(m, p.params.bits_per_sample);
  WriteParam(m, p.params.samples_per_packet);
}

bool ParamTraits<ViewHostMsg_Audio_CreateStream_Params>::Read(const Message* m,
                                                              void** iter,
                                                              param_type* p) {
  return
      ReadParam(m, iter, &p->params.format) &&
      ReadParam(m, iter, &p->params.channels) &&
      ReadParam(m, iter, &p->params.sample_rate) &&
      ReadParam(m, iter, &p->params.bits_per_sample) &&
      ReadParam(m, iter, &p->params.samples_per_packet);
}

void ParamTraits<ViewHostMsg_Audio_CreateStream_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<ViewHostMsg_Audio_CreateStream_Params>(");
  LogParam(p.params.format, l);
  l->append(", ");
  LogParam(p.params.channels, l);
  l->append(", ");
  LogParam(p.params.sample_rate, l);
  l->append(", ");
  LogParam(p.params.bits_per_sample, l);
  l->append(", ");
  LogParam(p.params.samples_per_packet, l);
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

void ParamTraits<ViewMsg_ExecuteCode_Params>::Write(Message* m,
                                                    const param_type& p) {
  WriteParam(m, p.request_id);
  WriteParam(m, p.extension_id);
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
  WriteParam(m, p.opener_frame_id);
  WriteParam(m, p.opener_url);
  WriteParam(m, p.opener_security_origin);
  WriteParam(m, p.target_url);
}

bool ParamTraits<ViewHostMsg_CreateWindow_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  return
      ReadParam(m, iter, &p->opener_id) &&
      ReadParam(m, iter, &p->user_gesture) &&
      ReadParam(m, iter, &p->window_container_type) &&
      ReadParam(m, iter, &p->session_storage_namespace_id) &&
      ReadParam(m, iter, &p->frame_name) &&
      ReadParam(m, iter, &p->opener_frame_id) &&
      ReadParam(m, iter, &p->opener_url) &&
      ReadParam(m, iter, &p->opener_security_origin) &&
      ReadParam(m, iter, &p->target_url);
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
  l->append(", ");
  LogParam(p.opener_frame_id, l);
  l->append(", ");
  LogParam(p.opener_url, l);
  l->append(", ");
  LogParam(p.opener_security_origin, l);
  l->append(", ");
  LogParam(p.target_url, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_RunFileChooser_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, static_cast<int>(p.mode));
  WriteParam(m, p.title);
  WriteParam(m, p.default_file_name);
  WriteParam(m, p.accept_types);
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
      ReadParam(m, iter, &p->default_file_name) &&
      ReadParam(m, iter, &p->accept_types);
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
  l->append(", ");
  LogParam(p.accept_types, l);
}

void ParamTraits<ViewMsg_ExtensionLoaded_Params>::Write(Message* m,
                                                        const param_type& p) {
  WriteParam(m, p.location);
  WriteParam(m, p.path);
  WriteParam(m, *(p.manifest));
}

bool ParamTraits<ViewMsg_ExtensionLoaded_Params>::Read(const Message* m,
                                                       void** iter,
                                                       param_type* p) {
  p->manifest.reset(new DictionaryValue());
  return ReadParam(m, iter, &p->location) &&
         ReadParam(m, iter, &p->path) &&
         ReadParam(m, iter, p->manifest.get());
}

void ParamTraits<ViewMsg_ExtensionLoaded_Params>::Log(const param_type& p,
                                                      std::string* l) {
  l->append(p.id);
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

void ParamTraits<base::FileUtilProxy::Entry>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.is_directory);
}

bool ParamTraits<base::FileUtilProxy::Entry>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->is_directory);
}

void ParamTraits<base::FileUtilProxy::Entry>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.is_directory, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_AccessibilityNotification_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.notification_type);
  WriteParam(m, p.acc_obj);
}

bool ParamTraits<ViewHostMsg_AccessibilityNotification_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->notification_type) &&
      ReadParam(m, iter, &p->acc_obj);
}

void ParamTraits<ViewHostMsg_AccessibilityNotification_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.notification_type, l);
  l->append(", ");
  LogParam(p.acc_obj, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_MalwareDOMDetails_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.nodes);
}

bool ParamTraits<ViewHostMsg_MalwareDOMDetails_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return ReadParam(m, iter, &p->nodes);
}

void ParamTraits<ViewHostMsg_MalwareDOMDetails_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.nodes, l);
  l->append(")");
}

void ParamTraits<ViewHostMsg_MalwareDOMDetails_Node>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.tag_name);
  WriteParam(m, p.parent);
  WriteParam(m, p.children);
}

bool ParamTraits<ViewHostMsg_MalwareDOMDetails_Node>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->tag_name) &&
      ReadParam(m, iter, &p->parent) &&
      ReadParam(m, iter, &p->children);
}

void ParamTraits<ViewHostMsg_MalwareDOMDetails_Node>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.tag_name, l);
  l->append(", ");
  LogParam(p.parent, l);
  l->append(", ");
  LogParam(p.children, l);
  l->append(")");
}

}  // namespace IPC
