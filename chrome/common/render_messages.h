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
#include "base/nullable_string16.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/sync_socket.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/nacl_types.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/view_types.h"
#include "chrome/common/web_apps.h"
#include "content/common/common_param_traits.h"
#include "content/common/css_colors.h"
#include "content/common/notification_type.h"
#include "content/common/page_transition_types.h"
#include "content/common/page_zoom.h"
#include "content/common/resource_response.h"
#include "chrome/common/web_apps.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webcursor.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

// TODO(mpcomplete): rename ViewMsg and ViewHostMsg to something that makes
// more sense with our current design.

// Singly-included section, not yet converted.
#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

// IPC_MESSAGE macros choke on extra , in the std::map, when expanding. We need
// to typedef it to avoid that.
// Substitution map for l10n messages.
typedef std::map<std::string, std::string> SubstitutionMap;

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

template <>
struct ParamTraits<URLPattern> {
  typedef URLPattern param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ExtensionExtent> {
  typedef ExtensionExtent param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_

#define IPC_MESSAGE_START ChromeMsgStart

IPC_ENUM_TRAITS(ContentSetting)
IPC_ENUM_TRAITS(ContentSettingsType)
IPC_ENUM_TRAITS(InstantCompleteBehavior)
IPC_ENUM_TRAITS(TranslateErrors::Type)
IPC_ENUM_TRAITS(ViewType::Type)
IPC_ENUM_TRAITS(WebKit::WebConsoleMessage::Level)

IPC_STRUCT_TRAITS_BEGIN(ThumbnailScore)
  IPC_STRUCT_TRAITS_MEMBER(boring_score)
  IPC_STRUCT_TRAITS_MEMBER(good_clipping)
  IPC_STRUCT_TRAITS_MEMBER(at_top)
  IPC_STRUCT_TRAITS_MEMBER(time_at_snapshot)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebApplicationInfo::IconInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebApplicationInfo)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(description)
  IPC_STRUCT_TRAITS_MEMBER(app_url)
  IPC_STRUCT_TRAITS_MEMBER(icons)
  IPC_STRUCT_TRAITS_MEMBER(permissions)
  IPC_STRUCT_TRAITS_MEMBER(launch_container)
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

// Notification that the user scripts have been updated. It has one
// SharedMemoryHandle argument consisting of the pickled script data. This
// handle is valid in the context of the renderer.
IPC_MESSAGE_CONTROL1(ViewMsg_UserScripts_UpdatedScripts,
                     base::SharedMemoryHandle)

// Sent when user prompting is required before a ViewHostMsg_GetCookies
// message can complete.  This message indicates that the renderer should
// pump messages while waiting for cookies.
IPC_MESSAGE_CONTROL0(ViewMsg_SignalCookiePromptEvent)

// RenderViewHostDelegate::RenderViewCreated method sends this message to a
// new renderer to notify it that it will host developer tools UI and should
// set up all neccessary bindings and create DevToolsClient instance that
// will handle communication with inspected page DevToolsAgent.
IPC_MESSAGE_ROUTED0(ViewMsg_SetupDevToolsClient)

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

// Install the first missing pluign.
IPC_MESSAGE_ROUTED0(ViewMsg_InstallMissingPlugin)

// Tells the renderer to empty its plugin list cache, optional reloading
// pages containing plugins.
IPC_MESSAGE_CONTROL1(ViewMsg_PurgePluginListCache,
                     bool /* reload_pages */)

// Tells the render view to load all blocked plugins.
IPC_MESSAGE_ROUTED0(ViewMsg_LoadBlockedPlugins)

// Tells the render view a prerendered page is about to be displayed.
IPC_MESSAGE_ROUTED0(ViewMsg_DisplayPrerenderedPage)

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

// Requests application info for the page. The renderer responds back with
// ViewHostMsg_DidGetApplicationInfo.
IPC_MESSAGE_ROUTED1(ViewMsg_GetApplicationInfo, int32 /*page_id*/)

// Requests the renderer to download the specified favicon image encode it as
// PNG and send the PNG data back ala ViewHostMsg_DidDownloadFavicon.
IPC_MESSAGE_ROUTED3(ViewMsg_DownloadFavicon,
                    int /* identifier for the request */,
                    GURL /* URL of the image */,
                    int /* Size of the image. Normally 0, but set if you have
                           a preferred image size to request, such as when
                           downloading the favicon */)

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

// Sent to the renderer when a popup window should no longer count against
// the current popup count (either because it's not a popup or because it was
// a generated by a user action or because a constrained popup got turned
// into a full window).
IPC_MESSAGE_ROUTED0(ViewMsg_DisassociateFromPopupCount)

// Sent by the Browser process to alert a window about whether a it should
// allow a scripted window.close(). The renderer assumes every new window is a
// blocked popup until notified otherwise.
IPC_MESSAGE_ROUTED1(ViewMsg_AllowScriptToClose,
                    bool /* script_can_close */)

// The browser sends this message in response to all extension api calls.
IPC_MESSAGE_ROUTED4(ViewMsg_ExtensionResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* response */,
                    std::string /* error */)

// This message is optionally routed.  If used as a control message, it
// will call a javascript function in every registered context in the
// target process.  If routed, it will be restricted to the contexts that
// are part of the target RenderView.
// If |extension_id| is non-empty, the function will be invoked only in
// contexts owned by the extension. |args| is a list of primitive Value types
// that are passed to the function.
IPC_MESSAGE_ROUTED4(ViewMsg_ExtensionMessageInvoke,
                    std::string /* extension_id */,
                    std::string /* function_name */,
                    ListValue /* args */,
                    GURL /* event URL */)

// Tell the renderer process all known extension function names.
IPC_MESSAGE_CONTROL1(ViewMsg_Extension_SetFunctionNames,
                     std::vector<std::string>)

// TODO(aa): SetAPIPermissions, SetHostPermissions, and possibly
// UpdatePageActions should be replaced with just sending additional data in
// ExtensionLoaded. See: crbug.com/70516.

// Tell the renderer process which permissions the given extension has. See
// Extension::Permissions for which elements correspond to which permissions.
IPC_MESSAGE_CONTROL2(ViewMsg_Extension_SetAPIPermissions,
                     std::string /* extension_id */,
                     std::set<std::string> /* permissions */)

// Tell the renderer process which host permissions the given extension has.
IPC_MESSAGE_CONTROL2(
    ViewMsg_Extension_SetHostPermissions,
    GURL /* source extension's origin */,
    std::vector<URLPattern> /* URLPatterns the extension can access */)

// Tell the renderer process all known page action ids for a particular
// extension.
IPC_MESSAGE_CONTROL2(ViewMsg_Extension_UpdatePageActions,
                     std::string /* extension_id */,
                     std::vector<std::string> /* page_action_ids */)

// Notifies the renderer that an extension was loaded in the browser.
IPC_MESSAGE_CONTROL1(ViewMsg_ExtensionLoaded, ViewMsg_ExtensionLoaded_Params)

// Notifies the renderer that an extension was unloaded in the browser.
IPC_MESSAGE_CONTROL1(ViewMsg_ExtensionUnloaded, std::string)

// Updates the scripting whitelist for extensions in the render process. This is
// only used for testing.
IPC_MESSAGE_CONTROL1(ViewMsg_Extension_SetScriptingWhitelist,
                     Extension::ScriptingWhitelist /* extenison ids */)

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

// Notification that renderer should run some JavaScript code.
IPC_MESSAGE_ROUTED1(ViewMsg_ExecuteCode,
                    ViewMsg_ExecuteCode_Params)

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

IPC_MESSAGE_CONTROL1(ViewHostMsg_UpdatedCacheStats,
                     WebKit::WebCache::UsageStats /* stats */)

// Tells the browser that content in the current page was blocked due to the
// user's content settings.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ContentBlocked,
                    ContentSettingsType, /* type of blocked content */
                    std::string /* resource identifier */)

// Used to get the extension message bundle.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetExtensionMessageBundle,
                            std::string /* extension id */,
                            SubstitutionMap /* message bundle */)

// Specifies the URL as the first parameter (a wstring) and thumbnail as
// binary data as the second parameter.
IPC_MESSAGE_ROUTED3(ViewHostMsg_Thumbnail,
                    GURL /* url */,
                    ThumbnailScore /* score */,
                    SkBitmap /* bitmap */)

// Send a snapshot of the tab contents to the render host.
IPC_MESSAGE_ROUTED1(ViewHostMsg_Snapshot,
                    SkBitmap /* bitmap */)

// Notification that the url for the favicon of a site has been determined.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateFaviconURL,
                    int32 /* page_id */,
                    GURL /* url of the favicon */)

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
                    ViewHostMsg_PageHasOSDD_Type)

// Find out if the given url's security origin is installed as a search
// provider.
IPC_SYNC_MESSAGE_ROUTED2_1(
    ViewHostMsg_GetSearchProviderInstallState,
    GURL /* page url */,
    GURL /* inquiry url */,
    ViewHostMsg_GetSearchProviderInstallState_Params /* install */)

// Stores new inspector setting in the profile.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateInspectorSetting,
                    std::string,  /* key */
                    std::string /* value */)

// Wraps an IPC message that's destined to the DevToolsClient on
// DevToolsAgent->browser hop.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardToDevToolsClient,
                    IPC::Message /* one of DevToolsClientMsg_XXX types */)

// Wraps an IPC message that's destined to the DevToolsAgent on
// DevToolsClient->browser hop.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardToDevToolsAgent,
                    IPC::Message /* one of DevToolsAgentMsg_XXX types */)

// Activates (brings to the front) corresponding dev tools window.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ActivateDevToolsWindow)

// Closes dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(ViewHostMsg_CloseDevToolsWindow)

// Attaches dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RequestDockDevToolsWindow)

// Detaches dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RequestUndockDevToolsWindow)

// Updates runtime features store in devtools manager in order to support
// cross-navigation instrumentation.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DevToolsRuntimePropertyChanged,
                    std::string /* name */,
                    std::string /* value */)

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

// Notifies when default plugin updates status of the missing plugin.
IPC_MESSAGE_ROUTED1(ViewHostMsg_MissingPluginStatus,
                    int /* status */)

// Notifies when a plugin couldn't be loaded because it's outdated.
IPC_MESSAGE_ROUTED2(ViewHostMsg_BlockedOutdatedPlugin,
                    string16, /* name */
                    GURL      /* update_url */)

// Displays a JavaScript out-of-memory message in the infobar.
IPC_MESSAGE_ROUTED0(ViewHostMsg_JSOutOfMemory)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                    std::vector<GURL> /* all savable resource links */,
                    std::vector<GURL> /* all referrers of resource links */,
                    std::vector<GURL> /* all frame links */)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendSerializedHtmlData,
                    GURL /* frame's url */,
                    std::string /* data buffer */,
                    int32 /* complete status */)

IPC_MESSAGE_ROUTED2(ViewHostMsg_DidGetApplicationInfo,
                    int32 /* page_id */,
                    WebApplicationInfo)

// Sent by the renderer to implement chrome.app.installApplication().
IPC_MESSAGE_ROUTED1(ViewHostMsg_InstallApplication,
                    WebApplicationInfo)

IPC_MESSAGE_ROUTED4(ViewHostMsg_DidDownloadFavicon,
                    int /* Identifier of the request */,
                    GURL /* URL of the image */,
                    bool /* true if there was a network error */,
                    SkBitmap /* image_data */)

// Provide the browser process with information about the WebCore resource
// cache.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ResourceTypeStats,
                     WebKit::WebCache::ResourceTypeStats)

// A renderer sends this message when an extension process starts an API
// request. The browser will always respond with a ViewMsg_ExtensionResponse.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ExtensionRequest,
                    ViewHostMsg_DomMessage_Params)

// Notify the browser that the given extension added a listener to an event.
IPC_MESSAGE_CONTROL2(ViewHostMsg_ExtensionAddListener,
                     std::string /* extension_id */,
                     std::string /* name */)

// Notify the browser that the given extension removed a listener from an
// event.
IPC_MESSAGE_CONTROL2(ViewHostMsg_ExtensionRemoveListener,
                     std::string /* extension_id */,
                     std::string /* name */)

// Message sent from renderer to the browser to update the state of a command.
// The |command| parameter is a RenderViewCommand. The |checked_state| parameter
// is a CommandCheckedState.
IPC_MESSAGE_ROUTED3(ViewHostMsg_CommandStateChanged,
                    int /* command */,
                    bool /* is_enabled */,
                    int /* checked_state */)

// Open a channel to all listening contexts owned by the extension with
// the given ID.  This always returns a valid port ID which can be used for
// sending messages.  If an error occurred, the opener will be notified
// asynchronously.
IPC_SYNC_MESSAGE_CONTROL4_1(ViewHostMsg_OpenChannelToExtension,
                            int /* routing_id */,
                            std::string /* source_extension_id */,
                            std::string /* target_extension_id */,
                            std::string /* channel_name */,
                            int /* port_id */)

// Get a port handle to the given tab.  The handle can be used for sending
// messages to the extension.
IPC_SYNC_MESSAGE_CONTROL4_1(ViewHostMsg_OpenChannelToTab,
                            int /* routing_id */,
                            int /* tab_id */,
                            std::string /* extension_id */,
                            std::string /* channel_name */,
                            int /* port_id */)

// Send a message to an extension process.  The handle is the value returned
// by ViewHostMsg_OpenChannelTo*.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ExtensionPostMessage,
                    int /* port_id */,
                    std::string /* message */)

// Send a message to an extension process.  The handle is the value returned
// by ViewHostMsg_OpenChannelTo*.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ExtensionCloseChannel,
                     int /* port_id */)

// Message sent from the renderer to the browser to request that the browser
// close all sockets.  Used for debugging/testing.
IPC_MESSAGE_CONTROL0(ViewHostMsg_CloseCurrentConnections)

// Message sent from the renderer to the browser to request that the browser
// enable or disable the cache.  Used for debugging/testing.
IPC_MESSAGE_CONTROL1(ViewHostMsg_SetCacheMode,
                     bool /* enabled */)

// Message sent from the renderer to the browser to request that the browser
// clear the cache.  Used for debugging/testing.
// |preserve_ssl_host_info| controls whether clearing the cache will preserve
// persisted SSL information stored in the cache.
// |result| is the returned status from the operation.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_ClearCache,
                            bool /* preserve_ssl_host_info */,
                            int  /* result */)

// Message sent from the renderer to the browser to request that the browser
// enable or disable spdy.  Used for debugging/testing/benchmarking.
IPC_MESSAGE_CONTROL1(ViewHostMsg_EnableSpdy,
                     bool /* enable */)

// Message sent from the renderer to the browser to request that the browser
// cache |data| associated with |url|.
IPC_MESSAGE_CONTROL3(ViewHostMsg_DidGenerateCacheableMetadata,
                     GURL /* url */,
                     double /* expected_response_time */,
                     std::vector<char> /* data */)

// Sent by the renderer process to acknowledge receipt of a
// ViewMsg_CSSInsertRequest message and css has been inserted into the frame.
IPC_MESSAGE_ROUTED0(ViewHostMsg_OnCSSInserted)

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

// Updates the content restrictions, i.e. to disable print/copy.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateContentRestrictions,
                    int /* restrictions */)

// The currently displayed PDF has an unsupported feature.
IPC_MESSAGE_ROUTED0(ViewHostMsg_PDFHasUnsupportedFeature)

// JavaScript related messages -----------------------------------------------

// Notify the JavaScript engine in the render to change its parameters
// while performing stress testing.
IPC_MESSAGE_ROUTED2(ViewMsg_JavaScriptStressTestControl,
                    int /* cmd */,
                    int /* param */)

// Register a new handler for URL requests with the given scheme.
IPC_MESSAGE_ROUTED3(ViewHostMsg_RegisterProtocolHandler,
                    std::string /* scheme */,
                    GURL /* url */,
                    string16 /* title */)

// Send from the renderer to the browser to return the script running result.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ExecuteCodeFinished,
                    int, /* request id */
                    bool /* whether the script ran successfully */)
