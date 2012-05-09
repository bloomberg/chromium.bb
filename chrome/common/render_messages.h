// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/nacl_types.h"
#include "chrome/common/search_provider.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/translate_errors.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/webkit_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "ui/gfx/rect.h"

// Singly-included section for enums and custom IPC traits.
#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

class SkBitmap;

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

// This enum is inside a struct so that we can forward-declare the struct in
// others headers without having to include this one.
struct ChromeViewHostMsg_GetPluginInfo_Status {
  enum Value {
    kAllowed,
    kBlocked,
    kClickToPlay,
    kDisabled,
    kNotFound,
    kOutdatedBlocked,
    kOutdatedDisallowed,
    kUnauthorized,
  };

  ChromeViewHostMsg_GetPluginInfo_Status() : value(kAllowed) {}

  Value value;
};

namespace IPC {

#if defined(OS_POSIX) && !defined(USE_AURA) && !defined(OS_ANDROID)

// TODO(port): this shouldn't exist. However, the plugin stuff is really using
// HWNDS (NativeView), and making Windows calls based on them. I've not figured
// out the deal with plugins yet.
// TODO(android): a gfx::NativeView is the same as a gfx::NativeWindow.
template <>
struct ParamTraits<gfx::NativeView> {
  typedef gfx::NativeView param_type;
  static void Write(Message* m, const param_type& p) {
    NOTIMPLEMENTED();
  }

  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
    NOTIMPLEMENTED();
    *p = NULL;
    return true;
  }

  static void Log(const param_type& p, std::string* l) {
    l->append(base::StringPrintf("<gfx::NativeView>"));
  }
};

#endif  // defined(OS_POSIX) && !defined(USE_AURA) && !defined(OS_ANDROID)

template <>
struct ParamTraits<ContentSettingsPattern> {
  typedef ContentSettingsPattern param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_

#define IPC_MESSAGE_START ChromeMsgStart

IPC_ENUM_TRAITS(ChromeViewHostMsg_GetPluginInfo_Status::Value)
IPC_ENUM_TRAITS(InstantCompleteBehavior)
IPC_ENUM_TRAITS(search_provider::OSDDType)
IPC_ENUM_TRAITS(search_provider::InstallState)
IPC_ENUM_TRAITS(TranslateErrors::Type)
IPC_ENUM_TRAITS(WebKit::WebConsoleMessage::Level)

IPC_STRUCT_TRAITS_BEGIN(ChromeViewHostMsg_GetPluginInfo_Status)
IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ContentSettingsPattern::PatternParts)
  IPC_STRUCT_TRAITS_MEMBER(scheme)
  IPC_STRUCT_TRAITS_MEMBER(is_scheme_wildcard)
  IPC_STRUCT_TRAITS_MEMBER(host)
  IPC_STRUCT_TRAITS_MEMBER(has_domain_wildcard)
  IPC_STRUCT_TRAITS_MEMBER(port)
  IPC_STRUCT_TRAITS_MEMBER(is_port_wildcard)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(is_path_wildcard)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ContentSettingPatternSource)
  IPC_STRUCT_TRAITS_MEMBER(primary_pattern)
  IPC_STRUCT_TRAITS_MEMBER(secondary_pattern)
  IPC_STRUCT_TRAITS_MEMBER(setting)
  IPC_STRUCT_TRAITS_MEMBER(source)
  IPC_STRUCT_TRAITS_MEMBER(incognito)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(RendererContentSettingRules)
  IPC_STRUCT_TRAITS_MEMBER(image_rules)
  IPC_STRUCT_TRAITS_MEMBER(script_rules)
IPC_STRUCT_TRAITS_END()

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
IPC_MESSAGE_CONTROL3(ChromeViewMsg_SetCacheCapacities,
                     size_t /* min_dead_capacity */,
                     size_t /* max_dead_capacity */,
                     size_t /* capacity */)

// Tells the renderer to clear the cache.
IPC_MESSAGE_CONTROL1(ChromeViewMsg_ClearCache,
                     bool /* on_navigation */)

// Tells the renderer to dump as much memory as it can, perhaps because we
// have memory pressure or the renderer is (or will be) paged out.  This
// should only result in purging objects we can recalculate, e.g. caches or
// JS garbage, not in purging irreplaceable objects.
IPC_MESSAGE_CONTROL0(ChromeViewMsg_PurgeMemory)

// For WebUI testing, this message stores parameters to do ScriptEvalRequest at
// a time which is late enough to not be thrown out, and early enough to be
// before onload events are fired.
IPC_MESSAGE_ROUTED4(ChromeViewMsg_WebUIJavaScript,
                    string16,  /* frame_xpath */
                    string16,  /* jscript_url */
                    int,  /* ID */
                    bool  /* If true, result is sent back. */)

// Tells the render view to capture a thumbnail image of the page. The
// render view responds with a ChromeViewHostMsg_Snapshot.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_CaptureSnapshot)

// History system notification that the visited link database has been
// replaced. It has one SharedMemoryHandle argument consisting of the table
// handle. This handle is valid in the context of the renderer
IPC_MESSAGE_CONTROL1(ChromeViewMsg_VisitedLink_NewTable,
                     base::SharedMemoryHandle)

// History system notification that a link has been added and the link
// coloring state for the given hash must be re-calculated.
IPC_MESSAGE_CONTROL1(ChromeViewMsg_VisitedLink_Add, std::vector<uint64>)

// History system notification that one or more history items have been
// deleted, which at this point means that all link coloring state must be
// re-calculated.
IPC_MESSAGE_CONTROL0(ChromeViewMsg_VisitedLink_Reset)

// Set the content setting rules stored by the renderer.
IPC_MESSAGE_CONTROL1(ChromeViewMsg_SetContentSettingRules,
                     RendererContentSettingRules /* rules */)

// Tells the render view to load all blocked plugins with the given identifier.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_LoadBlockedPlugins,
                    std::string /* identifier */)

// Asks the renderer to send back stats on the WebCore cache broken down by
// resource types.
IPC_MESSAGE_CONTROL0(ChromeViewMsg_GetCacheResourceStats)

// Asks the renderer to send back Histograms.
IPC_MESSAGE_CONTROL1(ChromeViewMsg_GetRendererHistograms,
                     int /* sequence number of Renderer Histograms. */)

// Tells the renderer to create a FieldTrial, and by using a 100% probability
// for the FieldTrial, forces the FieldTrial to have assigned group name.
IPC_MESSAGE_CONTROL2(ChromeViewMsg_SetFieldTrialGroup,
                     std::string /* field trial name */,
                     std::string /* group name that was assigned. */)

#if defined(USE_TCMALLOC)
// Asks the renderer to enable/disable Tcmalloc heap profiling.
// Note: filename_prefix arg is effectively ignored since the render process
// will be unable to write files to disk. Instead use WriteTcmallocHeapProfile
// to write a profile file.
IPC_MESSAGE_CONTROL2(ChromeViewMsg_SetTcmallocHeapProfiling,
                     bool /* enable profiling */,
                     std::string /* filename prefix for profiles */)
// Asks the renderer to write the Tcmalloc heap profile to a file.
IPC_MESSAGE_CONTROL1(ChromeViewMsg_WriteTcmallocHeapProfile,
                     FilePath::StringType /* filepath */)
#endif

// Asks the renderer to send back V8 heap stats.
IPC_MESSAGE_CONTROL0(ChromeViewMsg_GetV8HeapStats)

// Posts a message to the renderer.
IPC_MESSAGE_ROUTED3(ChromeViewMsg_HandleMessageFromExternalHost,
                    std::string /* The message */,
                    std::string /* The origin */,
                    std::string /* The target*/)

IPC_MESSAGE_ROUTED4(ChromeViewMsg_SearchBoxChange,
                    string16 /* value */,
                    bool /* verbatim */,
                    int /* selection_start */,
                    int /* selection_end */)
IPC_MESSAGE_ROUTED2(ChromeViewMsg_SearchBoxSubmit,
                    string16 /* value */,
                    bool /* verbatim */)
IPC_MESSAGE_ROUTED0(ChromeViewMsg_SearchBoxCancel)
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxResize,
                    gfx::Rect /* search_box_bounds */)
IPC_MESSAGE_ROUTED4(ChromeViewMsg_DetermineIfPageSupportsInstant,
                    string16 /* value*/,
                    bool /* verbatim */,
                    int /* selection_start */,
                    int /* selection_end */)

// Toggles visual muting of the render view area. This is on when a constrained
// window is showing.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetVisuallyDeemphasized,
                    bool /* deemphazied */)

// Tells the renderer to translate the page contents from one language to
// another.
IPC_MESSAGE_ROUTED4(ChromeViewMsg_TranslatePage,
                    int /* page id */,
                    std::string, /* the script injected in the page */
                    std::string, /* BCP 47/RFC 5646 language code the page
                                    is in */
                    std::string /* BCP 47/RFC 5646 language code to translate
                                   to */)

// Tells the renderer to revert the text of translated page to its original
// contents.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_RevertTranslation,
                    int /* page id */)

// Sent on process startup to indicate whether this process is running in
// incognito mode.
IPC_MESSAGE_CONTROL1(ChromeViewMsg_SetIsIncognitoProcess,
                     bool /* is_incognito_processs */)

// Sent in response to ViewHostMsg_DidBlockDisplayingInsecureContent.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetAllowDisplayingInsecureContent,
                    bool /* allowed */)

// Sent in response to ViewHostMsg_DidBlockRunningInsecureContent.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetAllowRunningInsecureContent,
                    bool /* allowed */)

// Tells renderer to always enforce mixed content blocking for this host.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_AddStrictSecurityHost,
                    std::string /* host */)

// Sent when the profile changes the kSafeBrowsingEnabled preference.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetClientSidePhishingDetection,
                    bool /* enable_phishing_detection */)

// This message asks frame sniffer start.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_StartFrameSniffer,
                    string16 /* frame-name */)

// JavaScript related messages -----------------------------------------------

// Notify the JavaScript engine in the render to change its parameters
// while performing stress testing.
IPC_MESSAGE_ROUTED2(ChromeViewMsg_JavaScriptStressTestControl,
                    int /* cmd */,
                    int /* param */)

// Asks the renderer to send back FPS.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_GetFPS)

// Tells the view it is displaying an interstitial page.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_SetAsInterstitial)

//-----------------------------------------------------------------------------
// Misc messages
// These are messages sent from the renderer to the browser process.

// Provides the contents for the given page that was loaded recently.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_PageContents,
                    GURL         /* URL of the page */,
                    int32        /* page id */,
                    string16     /* page contents */)

// Notification that the language for the tab has been determined.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_TranslateLanguageDetermined,
                    std::string  /* page ISO639_1 language code */,
                    bool         /* whether the page can be translated */)

IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_UpdatedCacheStats,
                     WebKit::WebCache::UsageStats /* stats */)

// Tells the browser that content in the current page was blocked due to the
// user's content settings.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_ContentBlocked,
                    ContentSettingsType, /* type of blocked content */
                    std::string /* resource identifier */)

// Sent by the renderer process to check whether access to web databases is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL5_1(ChromeViewHostMsg_AllowDatabase,
                            int /* render_view_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            string16 /* database name */,
                            string16 /* database display name */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to DOM Storage is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_AllowDOMStorage,
                            int /* render_view_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            bool /* if true local storage, otherwise session */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to FileSystem is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL3_1(ChromeViewHostMsg_AllowFileSystem,
                            int /* render_view_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to Indexed DBis
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_AllowIndexedDB,
                            int /* render_view_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            string16 /* database name */,
                            bool /* allowed */)

// Return information about a plugin for the given URL and MIME type.
// In contrast to ViewHostMsg_GetPluginInfo in content/, this IPC call knows
// about specific reasons why a plug-in can't be used, for example because it's
// disabled.
IPC_SYNC_MESSAGE_CONTROL4_3(ChromeViewHostMsg_GetPluginInfo,
                            int /* render_view_id */,
                            GURL /* url */,
                            GURL /* top origin url */,
                            std::string /* mime_type */,
                            ChromeViewHostMsg_GetPluginInfo_Status /* status */,
                            webkit::WebPluginInfo /* plugin */,
                            std::string /* actual_mime_type */)

#if defined(ENABLE_PLUGIN_INSTALLATION)
// Tells the browser to search for a plug-in that can handle the given MIME
// type. The result will be sent asynchronously to the routing ID
// |placeholder_id|.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_FindMissingPlugin,
                    int /* placeholder_id */,
                    std::string /* mime_type */)

// Notifies the browser that a missing plug-in placeholder has been removed, so
// the corresponding PluginPlaceholderHost can be deleted.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_RemovePluginPlaceholderHost,
                    int /* placeholder_id */)

// Notifies a missing plug-in placeholder that a plug-in with name |plugin_name|
// has been found.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_FoundMissingPlugin,
                    string16 /* plugin_name */)

// Notifies a missing plug-in placeholder that no plug-in has been found.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_DidNotFindMissingPlugin)

// Notifies a missing plug-in placeholder that we have started downloading
// the plug-in.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_StartedDownloadingPlugin)

// Notifies a missing plug-in placeholder that we have finished downloading
// the plug-in.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_FinishedDownloadingPlugin)

// Notifies a missing plug-in placeholder that there was an error downloading
// the plug-in.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_ErrorDownloadingPlugin,
                    std::string /* message */)
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

// Notifies a missing plug-in placeholder that the user cancelled downloading
// the plug-in.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_CancelledDownloadingPlugin)

// Tells the browser to open chrome://plugins in a new tab. We use a separate
// message because renderer processes aren't allowed to directly navigate to
// chrome:// URLs.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_OpenAboutPlugins)

// Specifies the URL as the first parameter (a wstring) and thumbnail as
// binary data as the second parameter.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_Thumbnail,
                    GURL /* url */,
                    ThumbnailScore /* score */,
                    SkBitmap /* bitmap */)

// Send a snapshot of the tab contents to the render host.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_Snapshot,
                    SkBitmap /* bitmap */)

// A message for an external host.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_ForwardMessageToExternalHost,
                    std::string  /* message */,
                    std::string  /* origin */,
                    std::string  /* target */)

// A renderer sends this to the browser process when it wants to start
// a new instance of the Native Client process. The browser will launch
// the process and return a handle to an IMC channel.
IPC_SYNC_MESSAGE_CONTROL2_1(ChromeViewHostMsg_LaunchNaCl,
                            GURL /* manifest_url */,
                            int /* socket count */,
                            std::vector<nacl::FileDescriptor>
                                /* imc channel handles */)

// Notification that the page has an OpenSearch description document
// associated with it.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_PageHasOSDD,
                    int32 /* page_id */,
                    GURL /* url of OS description document */,
                    search_provider::OSDDType)

// Find out if the given url's security origin is installed as a search
// provider.
IPC_SYNC_MESSAGE_ROUTED2_1(ChromeViewHostMsg_GetSearchProviderInstallState,
                           GURL /* page url */,
                           GURL /* inquiry url */,
                           search_provider::InstallState /* install */)

// Send back histograms as vector of pickled-histogram strings.
IPC_MESSAGE_CONTROL2(ChromeViewHostMsg_RendererHistograms,
                     int, /* sequence number of Renderer Histograms. */
                     std::vector<std::string>)

#if defined USE_TCMALLOC
// Send back tcmalloc profile to write to a file.
IPC_MESSAGE_CONTROL2(ChromeViewHostMsg_WriteTcmallocHeapProfile_ACK,
                     FilePath::StringType  /* filepath */,
                     std::string  /* heap profile */)
#endif

// Sends back stats about the V8 heap.
IPC_MESSAGE_CONTROL2(ChromeViewHostMsg_V8HeapStats,
                     int /* size of heap (allocated from the OS) */,
                     int /* bytes in use */)

// Request for a DNS prefetch of the names in the array.
// NameList is typedef'ed std::vector<std::string>
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_DnsPrefetch,
                     std::vector<std::string> /* hostnames */)

// Notifies when a plugin couldn't be loaded because it's outdated.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_BlockedOutdatedPlugin,
                    int /* placeholder ID */,
                    std::string /* plug-in group identifier */)

// Notifies when a plugin couldn't be loaded because it requires
// user authorization.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_BlockedUnauthorizedPlugin,
                    string16 /* name */,
                    std::string /* plug-in group identifier */)

// Provide the browser process with information about the WebCore resource
// cache and current renderer framerate.
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_ResourceTypeStats,
                     WebKit::WebCache::ResourceTypeStats)


// Notifies the browser of the language (ISO 639_1 code language, such as fr,
// en, zh...) of the current page.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_PageLanguageDetermined,
                    std::string /* the language */)

// Notifies the browser that a page has been translated.
IPC_MESSAGE_ROUTED4(ChromeViewHostMsg_PageTranslated,
                    int,                  /* page id */
                    std::string           /* the original language */,
                    std::string           /* the translated language */,
                    TranslateErrors::Type /* the error type if available */)

// Message sent from the renderer to the browser to notify it of events which
// may lead to the cancellation of a prerender. The message is sent only when
// the renderer is prerendering.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_MaybeCancelPrerenderForHTML5Media)

// Message sent from the renderer to the browser to notify it of a
// window.print() call which should cancel the prerender. The message is sent
// only when the renderer is prerendering.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_CancelPrerenderForPrinting)

// Sent by the renderer to check if a URL has permission to trigger a clipboard
// read/write operation from the DOM.
IPC_SYNC_MESSAGE_ROUTED1_1(ChromeViewHostMsg_CanTriggerClipboardRead,
                           GURL /* origin */,
                           bool /* allowed */)
IPC_SYNC_MESSAGE_ROUTED1_1(ChromeViewHostMsg_CanTriggerClipboardWrite,
                           GURL /* origin */,
                           bool /* allowed */)

// Sent when the renderer was prevented from displaying insecure content in
// a secure page by a security policy.  The page may appear incomplete.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_DidBlockDisplayingInsecureContent)

// Sent when the renderer was prevented from running insecure content in
// a secure origin by a security policy.  The page may appear incomplete.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_DidBlockRunningInsecureContent)

// Message sent from renderer to the browser when the element that is focused
// and currently accepts keyboard input inside the webpage has been touched.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_FocusedEditableNodeTouched)

// Suggest results -----------------------------------------------------------

IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_SetSuggestions,
                    int32 /* page_id */,
                    std::vector<std::string> /* suggestions */,
                    InstantCompleteBehavior)

IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_InstantSupportDetermined,
                    int32 /* page_id */,
                    bool  /* result */)

// The currently displayed PDF has an unsupported feature.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_PDFHasUnsupportedFeature)

// This message indicates the error appeared in the frame.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_FrameLoadingError,
                    int /* error */)

// This message indicates the monitored frame loading had completed.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_FrameLoadingCompleted)

// The following messages are used to set and get cookies for ChromeFrame
// processes.
// Used to set a cookie. The cookie is set asynchronously, but will be
// available to a subsequent ChromeViewHostMsg_GetCookies request.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_SetCookie,
                    GURL /* url */,
                    GURL /* first_party_for_cookies */,
                    std::string /* cookie */)

// Used to get cookies for the given URL. This may block waiting for a
// previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_ROUTED2_1(ChromeViewHostMsg_GetCookies,
                           GURL /* url */,
                           GURL /* first_party_for_cookies */,
                           std::string /* cookies */)

// Provide the browser process with current renderer framerate.
IPC_MESSAGE_CONTROL2(ChromeViewHostMsg_FPS,
                     int /* routing id */,
                     float /* frames per second */)
