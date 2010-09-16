// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_
#pragma once

#include <string>
#include <vector>

// TODO(erg): This list has been temporarily annotated by erg while doing work
// on which headers to pull out.
#include "app/clipboard/clipboard.h"                   // enum
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/css_colors.h"
#include "chrome/common/dom_storage_common.h"
#include "chrome/common/indexed_db_param_traits.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/resource_response.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/view_types.h"
#include "chrome/common/webkit_param_traits.h"
#include "gfx/native_widget_types.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"                     // ifdefed typedef.
#include "third_party/WebKit/WebKit/chromium/public/WebStorageArea.h"
#include "webkit/appcache/appcache_interfaces.h"  // enum appcache::Status
#include "webkit/glue/resource_loader_bridge.h"   // nested classes

#if defined(OS_MACOSX)
struct FontDescriptor;
#endif

namespace appcache {
struct AppCacheInfo;
struct AppCacheResourceInfo;
}

namespace base {
class Time;
}

namespace net {
class HttpResponseHeaders;
class UploadData;
}

namespace webkit_glue {
struct FormData;
class FormField;
struct PasswordFormFillData;
struct WebAccessibility;
struct WebCookie;
struct WebPluginGeometry;
struct WebAccessibility;
}

class ExtensionExtent;
class GURL;
class SkBitmap;
class URLPattern;
struct ContextMenuParams;
struct EditCommand;
struct RendererPreferences;
struct WebDropData;
struct WebMenuItem;
struct WebPluginInfo;
struct WebPluginMimeType;
struct WebPreferences;

// Forward declarations of structures used to store data for when we have a lot
// of parameters.
struct ViewMsg_Navigate_Params;
struct ViewMsg_AudioStreamState_Params;
struct ViewMsg_StopFinding_Params;
struct ViewHostMsg_GetSearchProviderInstallState_Params;
struct ViewHostMsg_FrameNavigate_Params;
struct ViewHostMsg_UpdateRect_Params;
struct ViewMsg_ClosePage_Params;
struct ViewHostMsg_Resource_Request;
struct ViewMsg_Print_Params;
struct ViewMsg_PrintPage_Params;
struct ViewMsg_PrintPages_Params;
struct ViewHostMsg_DidPrintPage_Params;
struct ViewHostMsg_Audio_CreateStream_Params;
struct ViewHostMsg_ShowPopup_Params;
struct ViewHostMsg_ScriptedPrint_Params;
struct ViewMsg_DOMStorageEvent_Params;
struct ViewHostMsg_IDBFactoryOpen_Params;
struct ViewHostMsg_IDBDatabaseCreateObjectStore_Params;
struct ViewHostMsg_IDBObjectStoreCreateIndex_Params;
struct ViewHostMsg_IDBObjectStoreOpenCursor_Params;
struct ViewMsg_ExecuteCode_Params;
struct ViewHostMsg_CreateWorker_Params;
struct ViewHostMsg_ShowNotification_Params;
struct ViewMsg_New_Params;
struct ViewHostMsg_CreateWindow_Params;
struct ViewHostMsg_RunFileChooser_Params;
struct ViewMsg_ExtensionRendererInfo;
struct ViewMsg_ExtensionsUpdated_Params;
struct ViewMsg_DeviceOrientationUpdated_Params;
struct ViewHostMsg_DomMessage_Params;
struct ViewHostMsg_OpenFileSystemRequest_Params;
struct ViewMsg_FileSystem_DidReadDirectory_Params;
struct ViewHostMsg_AccessibilityNotification_Params;

// Values that may be OR'd together to form the 'flags' parameter of the
// ViewMsg_EnablePreferredSizeChangedMode message.
enum ViewHostMsg_EnablePreferredSizeChangedMode_Flags {
  kPreferredSizeNothing,
  kPreferredSizeWidth = 1 << 0,
  // Requesting the height currently requires a polling loop in render_view.cc.
  kPreferredSizeHeightThisIsSlow = 1 << 1,
};

namespace IPC {

// Traits for FormField_Params structure to pack/unpack.
template <>
struct ParamTraits<webkit_glue::FormField> {
  typedef webkit_glue::FormField param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

#if defined(OS_MACOSX)
// Traits for FontDescriptor structure to pack/unpack.
template <>
struct ParamTraits<FontDescriptor> {
  typedef FontDescriptor param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};
#endif

template <>
struct ParamTraits<ContextMenuParams> {
  typedef ContextMenuParams param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit_glue::WebPluginGeometry> {
  typedef webkit_glue::WebPluginGeometry param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for ViewMsg_GetPlugins_Reply structure to pack/unpack.
template <>
struct ParamTraits<WebPluginMimeType> {
  typedef WebPluginMimeType param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebPluginInfo> {
  typedef WebPluginInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// Traits for webkit_glue::PasswordFormDomManager::FillData.
template <>
struct ParamTraits<webkit_glue::PasswordFormFillData> {
  typedef webkit_glue::PasswordFormFillData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<net::HttpResponseHeaders> > {
  typedef scoped_refptr<net::HttpResponseHeaders> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// Traits for webkit_glue::ResourceLoaderBridge::LoadTimingInfo
template <>
struct ParamTraits<webkit_glue::ResourceLoaderBridge::LoadTimingInfo> {
  typedef webkit_glue::ResourceLoaderBridge::LoadTimingInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.base_time.is_null());
    if (p.base_time.is_null())
      return;
    WriteParam(m, p.base_time);
    WriteParam(m, p.proxy_start);
    WriteParam(m, p.proxy_end);
    WriteParam(m, p.dns_start);
    WriteParam(m, p.dns_end);
    WriteParam(m, p.connect_start);
    WriteParam(m, p.connect_end);
    WriteParam(m, p.ssl_start);
    WriteParam(m, p.ssl_end);
    WriteParam(m, p.send_start);
    WriteParam(m, p.send_end);
    WriteParam(m, p.receive_headers_start);
    WriteParam(m, p.receive_headers_end);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool is_null;
    if (!ReadParam(m, iter, &is_null))
      return false;
    if (is_null)
      return true;

    return
        ReadParam(m, iter, &r->base_time) &&
        ReadParam(m, iter, &r->proxy_start) &&
        ReadParam(m, iter, &r->proxy_end) &&
        ReadParam(m, iter, &r->dns_start) &&
        ReadParam(m, iter, &r->dns_end) &&
        ReadParam(m, iter, &r->connect_start) &&
        ReadParam(m, iter, &r->connect_end) &&
        ReadParam(m, iter, &r->ssl_start) &&
        ReadParam(m, iter, &r->ssl_end) &&
        ReadParam(m, iter, &r->send_start) &&
        ReadParam(m, iter, &r->send_end) &&
        ReadParam(m, iter, &r->receive_headers_start) &&
        ReadParam(m, iter, &r->receive_headers_end);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.base_time, l);
    l->append(", ");
    LogParam(p.proxy_start, l);
    l->append(", ");
    LogParam(p.proxy_end, l);
    l->append(", ");
    LogParam(p.dns_start, l);
    l->append(", ");
    LogParam(p.dns_end, l);
    l->append(", ");
    LogParam(p.connect_start, l);
    l->append(", ");
    LogParam(p.connect_end, l);
    l->append(", ");
    LogParam(p.ssl_start, l);
    l->append(", ");
    LogParam(p.ssl_end, l);
    l->append(", ");
    LogParam(p.send_start, l);
    l->append(", ");
    LogParam(p.send_end, l);
    l->append(", ");
    LogParam(p.receive_headers_start, l);
    l->append(", ");
    LogParam(p.receive_headers_end, l);
    l->append(")");
  }
};

// Traits for webkit_glue::ResourceLoaderBridge::ResponseInfo
template <>
struct ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo> {
  typedef webkit_glue::ResourceLoaderBridge::ResponseInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.request_time);
    WriteParam(m, p.response_time);
    WriteParam(m, p.headers);
    WriteParam(m, p.mime_type);
    WriteParam(m, p.charset);
    WriteParam(m, p.security_info);
    WriteParam(m, p.content_length);
    WriteParam(m, p.appcache_id);
    WriteParam(m, p.appcache_manifest_url);
    WriteParam(m, p.connection_id);
    WriteParam(m, p.connection_reused);
    WriteParam(m, p.load_timing);
    WriteParam(m, p.download_file_path);
    WriteParam(m, p.was_fetched_via_spdy);
    WriteParam(m, p.was_npn_negotiated);
    WriteParam(m, p.was_alternate_protocol_available);
    WriteParam(m, p.was_fetched_via_proxy);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
        ReadParam(m, iter, &r->request_time) &&
        ReadParam(m, iter, &r->response_time) &&
        ReadParam(m, iter, &r->headers) &&
        ReadParam(m, iter, &r->mime_type) &&
        ReadParam(m, iter, &r->charset) &&
        ReadParam(m, iter, &r->security_info) &&
        ReadParam(m, iter, &r->content_length) &&
        ReadParam(m, iter, &r->appcache_id) &&
        ReadParam(m, iter, &r->appcache_manifest_url) &&
        ReadParam(m, iter, &r->connection_id) &&
        ReadParam(m, iter, &r->connection_reused) &&
        ReadParam(m, iter, &r->load_timing) &&
        ReadParam(m, iter, &r->download_file_path) &&
        ReadParam(m, iter, &r->was_fetched_via_spdy) &&
        ReadParam(m, iter, &r->was_npn_negotiated) &&
        ReadParam(m, iter, &r->was_alternate_protocol_available) &&
        ReadParam(m, iter, &r->was_fetched_via_proxy);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.request_time, l);
    l->append(", ");
    LogParam(p.response_time, l);
    l->append(", ");
    LogParam(p.headers, l);
    l->append(", ");
    LogParam(p.mime_type, l);
    l->append(", ");
    LogParam(p.charset, l);
    l->append(", ");
    LogParam(p.security_info, l);
    l->append(", ");
    LogParam(p.content_length, l);
    l->append(", ");
    LogParam(p.appcache_id, l);
    l->append(", ");
    LogParam(p.appcache_manifest_url, l);
    l->append(", ");
    LogParam(p.connection_id, l);
    l->append(", ");
    LogParam(p.connection_reused, l);
    l->append(", ");
    LogParam(p.load_timing, l);
    l->append(", ");
    LogParam(p.download_file_path, l);
    l->append(", ");
    LogParam(p.was_fetched_via_spdy, l);
    l->append(", ");
    LogParam(p.was_npn_negotiated, l);
    l->append(", ");
    LogParam(p.was_alternate_protocol_available, l);
    l->append(", ");
    LogParam(p.was_fetched_via_proxy, l);
    l->append(")");
  }
};

template <>
struct ParamTraits<ResourceResponseHead> {
  typedef ResourceResponseHead param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Write(m, p);
    WriteParam(m, p.status);
    WriteParam(m, p.replace_extension_localization_templates);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Read(
        m, iter, r) &&
        ReadParam(m, iter, &r->status) &&
        ReadParam(m, iter, &r->replace_extension_localization_templates);
  }
  static void Log(const param_type& p, std::string* l) {
    // log more?
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Log(p, l);
  }
};

template <>
struct ParamTraits<SyncLoadResult> {
  typedef SyncLoadResult param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<ResourceResponseHead>::Write(m, p);
    WriteParam(m, p.final_url);
    WriteParam(m, p.data);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ParamTraits<ResourceResponseHead>::Read(m, iter, r) &&
      ReadParam(m, iter, &r->final_url) &&
      ReadParam(m, iter, &r->data);
  }
  static void Log(const param_type& p, std::string* l) {
    // log more?
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Log(p, l);
  }
};

// Traits for FormData structure to pack/unpack.
template <>
struct ParamTraits<webkit_glue::FormData> {
  typedef webkit_glue::FormData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for reading/writing CSS Colors
template <>
struct ParamTraits<CSSColors::CSSColorName> {
  typedef CSSColors::CSSColorName param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, reinterpret_cast<int*>(p));
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<CSSColorName>");
  }
};

// Traits for RendererPreferences structure to pack/unpack.
template <>
struct ParamTraits<RendererPreferences> {
  typedef RendererPreferences param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for WebPreferences structure to pack/unpack.
template <>
struct ParamTraits<WebPreferences> {
  typedef WebPreferences param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for WebDropData
template <>
struct ParamTraits<WebDropData> {
  typedef WebDropData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

#if defined(OS_POSIX)

// TODO(port): this shouldn't exist. However, the plugin stuff is really using
// HWNDS (NativeView), and making Windows calls based on them. I've not figured
// out the deal with plugins yet.
template <>
struct ParamTraits<gfx::NativeView> {
  typedef gfx::NativeView param_type;
  static void Write(Message* m, const param_type& p) {
    NOTIMPLEMENTED();
  }

  static bool Read(const Message* m, void** iter, param_type* p) {
    NOTIMPLEMENTED();
    *p = NULL;
    return true;
  }

  static void Log(const param_type& p, std::string* l) {
    l->append(StringPrintf("<gfx::NativeView>"));
  }
};

#endif  // defined(OS_POSIX)

template <>
struct ParamTraits<appcache::Status> {
  typedef appcache::Status param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string state;
    switch (p) {
      case appcache::UNCACHED:
        state = "UNCACHED";
        break;
      case appcache::IDLE:
        state = "IDLE";
        break;
      case appcache::CHECKING:
        state = "CHECKING";
        break;
      case appcache::DOWNLOADING:
        state = "DOWNLOADING";
        break;
      case appcache::UPDATE_READY:
        state = "UPDATE_READY";
        break;
      case appcache::OBSOLETE:
        state = "OBSOLETE";
        break;
      default:
        state = "InvalidStatusValue";
        break;
    }

    LogParam(state, l);
  }
};

template <>
struct ParamTraits<appcache::EventID> {
  typedef appcache::EventID param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string state;
    switch (p) {
      case appcache::CHECKING_EVENT:
        state = "CHECKING_EVENT";
        break;
      case appcache::ERROR_EVENT:
        state = "ERROR_EVENT";
        break;
      case appcache::NO_UPDATE_EVENT:
        state = "NO_UPDATE_EVENT";
        break;
      case appcache::DOWNLOADING_EVENT:
        state = "DOWNLOADING_EVENT";
        break;
      case appcache::PROGRESS_EVENT:
        state = "PROGRESS_EVENT";
        break;
      case appcache::UPDATE_READY_EVENT:
        state = "UPDATE_READY_EVENT";
        break;
      case appcache::CACHED_EVENT:
        state = "CACHED_EVENT";
        break;
      case appcache::OBSOLETE_EVENT:
        state = "OBSOLETE_EVENT";
        break;
      default:
        state = "InvalidEventValue";
        break;
    }

    LogParam(state, l);
  }
};

template<>
struct ParamTraits<WebMenuItem> {
  typedef WebMenuItem param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<ViewType::Type> {
  typedef int Type;
};

// Traits for URLPattern.
template <>
struct ParamTraits<URLPattern> {
  typedef URLPattern param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<Clipboard::Buffer> {
  typedef Clipboard::Buffer param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int buffer;
    if (!m->ReadInt(iter, &buffer) || !Clipboard::IsValidBuffer(buffer))
      return false;
    *p = Clipboard::FromInt(buffer);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string type;
    switch (p) {
      case Clipboard::BUFFER_STANDARD:
        type = "BUFFER_STANDARD";
        break;
#if defined(USE_X11)
      case Clipboard::BUFFER_SELECTION:
        type = "BUFFER_SELECTION";
        break;
#endif
      case Clipboard::BUFFER_DRAG:
        type = "BUFFER_DRAG";
        break;
      default:
        type = "UNKNOWN";
        break;
    }

    LogParam(type, l);
  }
};

// Traits for EditCommand structure.
template <>
struct ParamTraits<EditCommand> {
  typedef EditCommand param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for DOMStorageType enum.
template <>
struct ParamTraits<DOMStorageType> {
  typedef DOMStorageType param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string control;
    switch (p) {
      case DOM_STORAGE_LOCAL:
        control = "DOM_STORAGE_LOCAL";
        break;
      case DOM_STORAGE_SESSION:
        control = "DOM_STORAGE_SESSION";
        break;
      default:
        NOTIMPLEMENTED();
        control = "UNKNOWN";
        break;
    }
    LogParam(control, l);
  }
};

// Traits for WebKit::WebStorageArea::Result enum.
template <>
struct ParamTraits<WebKit::WebStorageArea::Result> {
  typedef WebKit::WebStorageArea::Result param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string control;
    switch (p) {
      case WebKit::WebStorageArea::ResultOK:
        control = "WebKit::WebStorageArea::ResultOK";
        break;
      case WebKit::WebStorageArea::ResultBlockedByQuota:
        control = "WebKit::WebStorageArea::ResultBlockedByQuota";
        break;
      case WebKit::WebStorageArea::ResultBlockedByPolicy:
        control = "WebKit::WebStorageArea::ResultBlockedByPolicy";
        break;
      default:
        NOTIMPLEMENTED();
        control = "UNKNOWN";
        break;
    }
    LogParam(control, l);
  }
};

// Traits for WebCookie
template <>
struct ParamTraits<webkit_glue::WebCookie> {
  typedef webkit_glue::WebCookie param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<TranslateErrors::Type> {
  typedef int Type;
};

template <>
struct ParamTraits<ExtensionExtent> {
  typedef ExtensionExtent param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};


template<>
struct ParamTraits<appcache::AppCacheResourceInfo> {
  typedef appcache::AppCacheResourceInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<appcache::AppCacheInfo> {
  typedef appcache::AppCacheInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit_glue::WebAccessibility> {
  typedef webkit_glue::WebAccessibility param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for base::PlatformFileError
template <>
struct SimilarTypeTraits<base::PlatformFileError> {
  typedef int Type;
};

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/render_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_
