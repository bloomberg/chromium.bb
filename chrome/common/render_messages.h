// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_
#pragma once

#include <string>
#include <vector>

// TODO(erg): This list has been temporarily annotated by erg while doing work
// on which headers to pull out.
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/css_colors.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/view_types.h"
#include "chrome/common/webkit_param_traits.h"
#include "content/common/common_param_traits.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"                     // ifdefed typedef.
#include "ui/base/clipboard/clipboard.h"                   // enum
#include "webkit/appcache/appcache_interfaces.h"  // enum appcache::Status
#include "webkit/fileapi/file_system_types.h"  // enum fileapi::FileSystemType

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

namespace webkit_blob {
class BlobData;
}

namespace webkit_glue {
struct CustomContextMenuContext;
struct WebAccessibility;
struct WebCookie;
}

namespace webkit {
namespace npapi {
struct WebPluginGeometry;
struct WebPluginInfo;
struct WebPluginMimeType;
}
}

struct AudioBuffersState;
class ExtensionExtent;
class GURL;
class SkBitmap;
class URLPattern;
struct ContextMenuParams;
struct EditCommand;
struct RendererPreferences;
struct WebDropData;
struct WebMenuItem;
struct WebPreferences;

// Forward declarations of structures used to store data for when we have a lot
// of parameters.
struct ViewHostMsg_AccessibilityNotification_Params;
struct ViewHostMsg_Audio_CreateStream_Params;
struct ViewHostMsg_CreateWindow_Params;
struct ViewHostMsg_CreateWorker_Params;
struct ViewHostMsg_DidPreviewDocument_Params;
struct ViewHostMsg_DidPrintPage_Params;
struct ViewHostMsg_DomMessage_Params;
struct ViewHostMsg_FrameNavigate_Params;
struct ViewHostMsg_GetSearchProviderInstallState_Params;
struct ViewHostMsg_MalwareDOMDetails_Params;
struct ViewHostMsg_PageHasOSDD_Type;
struct ViewHostMsg_RunFileChooser_Params;
struct ViewHostMsg_ShowNotification_Params;
struct ViewHostMsg_ShowPopup_Params;
struct ViewHostMsg_ScriptedPrint_Params;
struct ViewHostMsg_UpdateRect_Params;
struct ViewMsg_AudioStreamState_Params;
struct ViewMsg_ClosePage_Params;
struct ViewMsg_DeviceOrientationUpdated_Params;
struct ViewMsg_ExecuteCode_Params;
struct ViewMsg_ExtensionLoaded_Params;
struct ViewMsg_New_Params;
struct ViewMsg_Navigate_Params;
struct ViewMsg_Print_Params;
struct ViewMsg_PrintPages_Params;
struct ViewMsg_PrintPage_Params;
struct ViewMsg_StopFinding_Params;

// Values that may be OR'd together to form the 'flags' parameter of the
// ViewMsg_EnablePreferredSizeChangedMode message.
enum ViewHostMsg_EnablePreferredSizeChangedMode_Flags {
  kPreferredSizeNothing,
  kPreferredSizeWidth = 1 << 0,
  // Requesting the height currently requires a polling loop in render_view.cc.
  kPreferredSizeHeightThisIsSlow = 1 << 1,
};

// Command values for the cmd parameter of the
// ViewHost_JavaScriptStressTestControl message. For each command the parameter
// passed has a different meaning:
// For the command kJavaScriptStressTestSetStressRunType the parameter it the
// type taken from the enumeration v8::Testing::StressType.
// For the command kJavaScriptStressTestPrepareStressRun the parameter it the
// number of the stress run about to take place.
enum ViewHostMsg_JavaScriptStressTestControl_Commands {
  kJavaScriptStressTestSetStressRunType = 0,
  kJavaScriptStressTestPrepareStressRun = 1,
};

namespace IPC {

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
struct ParamTraits<webkit_glue::CustomContextMenuContext> {
  typedef webkit_glue::CustomContextMenuContext param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ContextMenuParams> {
  typedef ContextMenuParams param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit::npapi::WebPluginGeometry> {
  typedef webkit::npapi::WebPluginGeometry param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for ViewMsg_GetPlugins_Reply structure to pack/unpack.
template <>
struct ParamTraits<webkit::npapi::WebPluginMimeType> {
  typedef webkit::npapi::WebPluginMimeType param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit::npapi::WebPluginInfo> {
  typedef webkit::npapi::WebPluginInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
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
    l->append(base::StringPrintf("<gfx::NativeView>"));
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
struct ParamTraits<ui::Clipboard::Buffer> {
  typedef ui::Clipboard::Buffer param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int buffer;
    if (!m->ReadInt(iter, &buffer) || !ui::Clipboard::IsValidBuffer(buffer))
      return false;
    *p = ui::Clipboard::FromInt(buffer);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string type;
    switch (p) {
      case ui::Clipboard::BUFFER_STANDARD:
        type = "BUFFER_STANDARD";
        break;
#if defined(USE_X11)
      case ui::Clipboard::BUFFER_SELECTION:
        type = "BUFFER_SELECTION";
        break;
#endif
      case ui::Clipboard::BUFFER_DRAG:
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

template <>
struct ParamTraits<scoped_refptr<webkit_blob::BlobData> > {
  typedef scoped_refptr<webkit_blob::BlobData> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<fileapi::FileSystemType> {
  typedef int Type;
};

// Traits for AudioBuffersState structure.
template <>
struct ParamTraits<AudioBuffersState> {
  typedef AudioBuffersState param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#include "chrome/common/render_messages_internal.h"

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_
