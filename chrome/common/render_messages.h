// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <map>
#include <set>
#include <string>
#include <vector>

// TODO(erg): This list has been temporarily annotated by erg while doing work
// on which headers to pull out.
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/nacl_types.h"
#include "chrome/common/search_provider.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/view_types.h"
#include "content/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

// Singly-included section for enums and custom IPC traits.
#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

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
struct ParamTraits<ContentSettings> {
  typedef ContentSettings param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_

#define IPC_MESSAGE_START ChromeMsgStart

IPC_ENUM_TRAITS(InstantCompleteBehavior)
IPC_ENUM_TRAITS(search_provider::OSDDType)
IPC_ENUM_TRAITS(search_provider::InstallState)
IPC_ENUM_TRAITS(TranslateErrors::Type)
IPC_ENUM_TRAITS(ViewType::Type)
IPC_ENUM_TRAITS(WebKit::WebConsoleMessage::Level)

IPC_STRUCT_TRAITS_BEGIN(ThumbnailScore)
  IPC_STRUCT_TRAITS_MEMBER(boring_score)
  IPC_STRUCT_TRAITS_MEMBER(good_clipping)
  IPC_STRUCT_TRAITS_MEMBER(at_top)
  IPC_STRUCT_TRAITS_MEMBER(time_at_snapshot)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebCache::ResourceTypeStat)
  IPC_STRUCT_TRAITS_MEMBER(count)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(liveSize)
  IPC_STRUCT_TRAITS_MEMBER(decodedSize)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebCache::ResourceTypeStats)
  IPC_STRUCT_TRAITS_MEMBER(images)
  IPC_STRUCT_TRAITS_MEMBER(cssStyleSheets)
  IPC_STRUCT_TRAITS_MEMBER(scripts)
  IPC_STRUCT_TRAITS_MEMBER(xslStyleSheets)
  IPC_STRUCT_TRAITS_MEMBER(fonts)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebCache::UsageStats)
  IPC_STRUCT_TRAITS_MEMBER(minDeadCapacity)
  IPC_STRUCT_TRAITS_MEMBER(maxDeadCapacity)
  IPC_STRUCT_TRAITS_MEMBER(capacity)
  IPC_STRUCT_TRAITS_MEMBER(liveSize)
  IPC_STRUCT_TRAITS_MEMBER(deadSize)
IPC_STRUCT_TRAITS_END()

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Tells the renderer to set its maximum cache size to the supplied value.
IPC_MESSAGE_CONTROL3(ViewMsg_SetCacheCapacities,
                     size_t /* min_dead_capacity */,
                     size_t /* max_dead_capacity */,
                     size_t /* capacity */)

// Tells the renderer to clear the cache.
IPC_MESSAGE_CONTROL0(ViewMsg_ClearCache)

// Tells the renderer to dump as much memory as it can, perhaps because we
// have memory pressure or the renderer is (or will be) paged out.  This
// should only result in purging objects we can recalculate, e.g. caches or
// JS garbage, not in purging irreplaceable objects.
IPC_MESSAGE_CONTROL0(ViewMsg_PurgeMemory)

// Tells the render view to capture a thumbnail image of the page. The
// render view responds with a ViewHostMsg_Snapshot.
IPC_MESSAGE_ROUTED0(ViewMsg_CaptureSnapshot)

// History system notification that the visited link database has been
// replaced. It has one SharedMemoryHandle argument consisting of the table
// handle. This handle is valid in the context of the renderer
IPC_MESSAGE_CONTROL1(ViewMsg_VisitedLink_NewTable, base::SharedMemoryHandle)

// History system notification that a link has been added and the link
// coloring state for the given hash must be re-calculated.
IPC_MESSAGE_CONTROL1(ViewMsg_VisitedLink_Add, std::vector<uint64>)

// History system notification that one or more history items have been
// deleted, which at this point means that all link coloring state must be
// re-calculated.
IPC_MESSAGE_CONTROL0(ViewMsg_VisitedLink_Reset)

// Set the content settings for a particular url that the renderer is in the
// process of loading.  This will be stored, to be used if the load commits
// and ignored otherwise.
IPC_MESSAGE_ROUTED2(ViewMsg_SetContentSettingsForLoadingURL,
                    GURL /* url */,
                    ContentSettings /* content_settings */)

// Set the content settings for a particular url, so all render views
// displaying this host url update their content settings to match.
IPC_MESSAGE_CONTROL2(ViewMsg_SetContentSettingsForCurrentURL,
                     GURL /* url */,
                     ContentSettings /* content_settings */)

// Tells the render view to load all blocked plugins.
IPC_MESSAGE_ROUTED0(ViewMsg_LoadBlockedPlugins)

// Used to instruct the RenderView to go into "view source" mode.
IPC_MESSAGE_ROUTED0(ViewMsg_EnableViewSourceMode)

// Get all savable resource links from current webpage, include main
// frame and sub-frame.
IPC_MESSAGE_ROUTED1(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                    GURL /* url of page which is needed to save */)

// Get html data by serializing all frames of current page with lists
// which contain all resource links that have local copy.
IPC_MESSAGE_ROUTED3(ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
                    std::vector<GURL> /* urls that have local copy */,
                    std::vector<FilePath> /* paths of local copy */,
                    FilePath /* local directory path */)

// Asks the renderer to send back stats on the WebCore cache broken down by
// resource types.
IPC_MESSAGE_CONTROL0(ViewMsg_GetCacheResourceStats)

// Asks the renderer to send back Histograms.
IPC_MESSAGE_CONTROL1(ViewMsg_GetRendererHistograms,
                     int /* sequence number of Renderer Histograms. */)

#if defined(USE_TCMALLOC)
// Asks the renderer to send back tcmalloc stats.
IPC_MESSAGE_CONTROL0(ViewMsg_GetRendererTcmalloc)
#endif

// Asks the renderer to send back V8 heap stats.
IPC_MESSAGE_CONTROL0(ViewMsg_GetV8HeapStats)

// Posts a message to the renderer.
IPC_MESSAGE_ROUTED3(ViewMsg_HandleMessageFromExternalHost,
                    std::string /* The message */,
                    std::string /* The origin */,
                    std::string /* The target*/)

IPC_MESSAGE_ROUTED4(ViewMsg_SearchBoxChange,
                    string16 /* value */,
                    bool /* verbatim */,
                    int /* selection_start */,
                    int /* selection_end */)
IPC_MESSAGE_ROUTED2(ViewMsg_SearchBoxSubmit,
                    string16 /* value */,
                    bool /* verbatim */)
IPC_MESSAGE_ROUTED0(ViewMsg_SearchBoxCancel)
IPC_MESSAGE_ROUTED1(ViewMsg_SearchBoxResize,
                    gfx::Rect /* search_box_bounds */)
IPC_MESSAGE_ROUTED4(ViewMsg_DetermineIfPageSupportsInstant,
                    string16 /* value*/,
                    bool /* verbatim */,
                    int /* selection_start */,
                    int /* selection_end */)

// Tell the renderer which browser window it's being attached to.
IPC_MESSAGE_ROUTED1(ViewMsg_UpdateBrowserWindowId,
                    int /* id of browser window */)

// Tell the renderer which type this view is.
IPC_MESSAGE_ROUTED1(ViewMsg_NotifyRenderViewType,
                    ViewType::Type /* view_type */)

// Tells the renderer to translate the page contents from one language to
// another.
IPC_MESSAGE_ROUTED4(ViewMsg_TranslatePage,
                    int /* page id */,
                    std::string, /* the script injected in the page */
                    std::string, /* BCP 47/RFC 5646 language code the page
                                    is in */
                    std::string /* BCP 47/RFC 5646 language code to translate
                                   to */)

// Tells the renderer to revert the text of translated page to its original
// contents.
IPC_MESSAGE_ROUTED1(ViewMsg_RevertTranslation,
                    int /* page id */)

// Sent on process startup to indicate whether this process is running in
// incognito mode.
IPC_MESSAGE_CONTROL1(ViewMsg_SetIsIncognitoProcess,
                     bool /* is_incognito_processs */)

//-----------------------------------------------------------------------------
// TabContents messages
// These are messages sent from the renderer to the browser process.

// Provides the contents for the given page that was loaded recently.
IPC_MESSAGE_ROUTED3(ViewHostMsg_PageContents,
                    GURL         /* URL of the page */,
                    int32        /* page id */,
                    string16     /* page contents */)

// Notification that the language for the tab has been determined.
IPC_MESSAGE_ROUTED2(ViewHostMsg_TranslateLanguageDetermined,
                    std::string  /* page ISO639_1 language code */,
                    bool         /* whether the page can be translated */)

IPC_MESSAGE_CONTROL1(ViewHostMsg_UpdatedCacheStats,
                     WebKit::WebCache::UsageStats /* stats */)

// Tells the browser that content in the current page was blocked due to the
// user's content settings.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ContentBlocked,
                    ContentSettingsType, /* type of blocked content */
                    std::string /* resource identifier */)

// Specifies the URL as the first parameter (a wstring) and thumbnail as
// binary data as the second parameter.
IPC_MESSAGE_ROUTED3(ViewHostMsg_Thumbnail,
                    GURL /* url */,
                    ThumbnailScore /* score */,
                    SkBitmap /* bitmap */)

// Send a snapshot of the tab contents to the render host.
IPC_MESSAGE_ROUTED1(ViewHostMsg_Snapshot,
                    SkBitmap /* bitmap */)

// Following message is used to communicate the values received by the
// callback binding the JS to Cpp.
// An instance of browser that has an automation host listening to it can
// have a javascript send a native value (string, number, boolean) to the
// listener in Cpp. (DomAutomationController)
IPC_MESSAGE_ROUTED2(ViewHostMsg_DomOperationResponse,
                    std::string  /* json_string */,
                    int  /* automation_id */)

// A message for an external host.
IPC_MESSAGE_ROUTED3(ViewHostMsg_ForwardMessageToExternalHost,
                    std::string  /* message */,
                    std::string  /* origin */,
                    std::string  /* target */)

// A renderer sends this to the browser process when it wants to start
// a new instance of the Native Client process. The browser will launch
// the process and return a handle to an IMC channel.
IPC_SYNC_MESSAGE_CONTROL2_3(ViewHostMsg_LaunchNaCl,
                            std::wstring /* url for the NaCl module */,
                            int /* socket count */,
                            std::vector<nacl::FileDescriptor>
                                /* imc channel handles */,
                            base::ProcessHandle /* NaCl process handle */,
                            base::ProcessId /* NaCl process id */)

// Notification that the page has an OpenSearch description document
// associated with it.
IPC_MESSAGE_ROUTED3(ViewHostMsg_PageHasOSDD,
                    int32 /* page_id */,
                    GURL /* url of OS description document */,
                    search_provider::OSDDType)

// Find out if the given url's security origin is installed as a search
// provider.
IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_GetSearchProviderInstallState,
                           GURL /* page url */,
                           GURL /* inquiry url */,
                           search_provider::InstallState /* install */)

// Send back a string to be recorded by UserMetrics.
IPC_MESSAGE_CONTROL1(ViewHostMsg_UserMetricsRecordAction,
                     std::string /* action */)

// Send back histograms as vector of pickled-histogram strings.
IPC_MESSAGE_CONTROL2(ViewHostMsg_RendererHistograms,
                     int, /* sequence number of Renderer Histograms. */
                     std::vector<std::string>)

#if defined USE_TCMALLOC
// Send back tcmalloc stats output.
IPC_MESSAGE_CONTROL2(ViewHostMsg_RendererTcmalloc,
                     int          /* pid */,
                     std::string  /* tcmalloc debug output */)
#endif

// Sends back stats about the V8 heap.
IPC_MESSAGE_CONTROL2(ViewHostMsg_V8HeapStats,
                     int /* size of heap (allocated from the OS) */,
                     int /* bytes in use */)

// Request for a DNS prefetch of the names in the array.
// NameList is typedef'ed std::vector<std::string>
IPC_MESSAGE_CONTROL1(ViewHostMsg_DnsPrefetch,
                     std::vector<std::string> /* hostnames */)

// Requests the outdated plugins policy.
// |policy| is one of ALLOW, BLOCK or ASK. Anything else is an error.
// ALLOW means that outdated plugins are allowed, and BLOCK that they should
// be blocked. The default is ASK, which blocks the plugin initially but allows
// the user to start them manually.
IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_GetOutdatedPluginsPolicy,
                           ContentSetting   /* policy */)

// Notifies when a plugin couldn't be loaded because it's outdated.
IPC_MESSAGE_ROUTED2(ViewHostMsg_BlockedOutdatedPlugin,
                    string16, /* name */
                    GURL      /* update_url */)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                    std::vector<GURL> /* all savable resource links */,
                    std::vector<GURL> /* all referrers of resource links */,
                    std::vector<GURL> /* all frame links */)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendSerializedHtmlData,
                    GURL /* frame's url */,
                    std::string /* data buffer */,
                    int32 /* complete status */)

// Provide the browser process with information about the WebCore resource
// cache.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ResourceTypeStats,
                     WebKit::WebCache::ResourceTypeStats)

// Message sent from renderer to the browser to update the state of a command.
// The |command| parameter is a RenderViewCommand. The |checked_state| parameter
// is a CommandCheckedState.
IPC_MESSAGE_ROUTED3(ViewHostMsg_CommandStateChanged,
                    int /* command */,
                    bool /* is_enabled */,
                    int /* checked_state */)


// Notifies the browser of the language (ISO 639_1 code language, such as fr,
// en, zh...) of the current page.
IPC_MESSAGE_ROUTED1(ViewHostMsg_PageLanguageDetermined,
                    std::string /* the language */)

// Notifies the browser that a page has been translated.
IPC_MESSAGE_ROUTED4(ViewHostMsg_PageTranslated,
                    int,                  /* page id */
                    std::string           /* the original language */,
                    std::string           /* the translated language */,
                    TranslateErrors::Type /* the error type if available */)

// Suggest results -----------------------------------------------------------

IPC_MESSAGE_ROUTED3(ViewHostMsg_SetSuggestions,
                    int32 /* page_id */,
                    std::vector<std::string> /* suggestions */,
                    InstantCompleteBehavior)

IPC_MESSAGE_ROUTED2(ViewHostMsg_InstantSupportDetermined,
                    int32 /* page_id */,
                    bool  /* result */)

// JavaScript related messages -----------------------------------------------

// Notify the JavaScript engine in the render to change its parameters
// while performing stress testing.
IPC_MESSAGE_ROUTED2(ViewMsg_JavaScriptStressTestControl,
                    int /* cmd */,
                    int /* param */)
