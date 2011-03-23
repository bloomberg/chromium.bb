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
#include "chrome/common/common_param_traits.h"
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
#include "chrome/common/webkit_param_traits.h"
#include "content/common/common_param_traits.h"
#include "content/common/css_colors.h"
#include "content/common/notification_type.h"
#include "content/common/page_transition_types.h"
#include "content/common/page_zoom.h"
#include "content/common/resource_response.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"                     // ifdefed typedef.
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webaccessibility.h"
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
struct SimilarTypeTraits<TranslateErrors::Type> {
  typedef int Type;
};

template <>
struct SimilarTypeTraits<InstantCompleteBehavior> {
  typedef int Type;
};

template <>
struct ParamTraits<ExtensionExtent> {
  typedef ExtensionExtent param_type;
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

}  // namespace IPC

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_

#define IPC_MESSAGE_START ChromeMsgStart

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

IPC_MESSAGE_ROUTED0(ViewMsg_PrintNodeUnderContextMenu)

// Tells the renderer to print the print preview tab's PDF plugin without
// showing the print dialog.
IPC_MESSAGE_ROUTED1(ViewMsg_PrintForPrintPreview,
                    DictionaryValue /* settings*/)

// Tells the render view to switch the CSS to print media type, renders every
// requested pages and switch back the CSS to display media type.
IPC_MESSAGE_ROUTED0(ViewMsg_PrintPages)

// Tells the render view that printing is done so it can clean up.
IPC_MESSAGE_ROUTED2(ViewMsg_PrintingDone,
                    int /* document_cookie */,
                    bool /* success */)

// Tells the render view to switch the CSS to print media type, renders every
// requested pages for print preview using the given |settngs|.
IPC_MESSAGE_ROUTED1(ViewMsg_PrintPreview,
                    DictionaryValue /* settings */)

// Tells a renderer to stop blocking script initiated printing.
IPC_MESSAGE_ROUTED0(ViewMsg_ResetScriptedPrintCount)

// Sends back to the browser the rendered "printed document" for preview that
// was requested by a ViewMsg_PrintPreview message. The memory handle in this
// message is already valid in the browser process.
IPC_MESSAGE_ROUTED1(ViewHostMsg_PagesReadyForPreview,
                    ViewHostMsg_DidPreviewDocument_Params /* params */)

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

// SpellChecker messages.

IPC_MESSAGE_ROUTED0(ViewMsg_ToggleSpellCheck)
IPC_MESSAGE_ROUTED1(ViewMsg_ToggleSpellPanel,
                    bool)
IPC_MESSAGE_ROUTED3(ViewMsg_SpellChecker_RespondTextCheck,
                    int        /* request identifier given by WebKit */,
                    int        /* document tag */,
                    std::vector<WebKit::WebTextCheckingResult>)

// This message tells the renderer to advance to the next misspelling. It is
// sent when the user clicks the "Find Next" button on the spelling panel.
IPC_MESSAGE_ROUTED0(ViewMsg_AdvanceToNextMisspelling)

// Passes some initialization params to the renderer's spellchecker. This can
// be called directly after startup or in (async) response to a
// RequestDictionary ViewHost message.
IPC_MESSAGE_CONTROL4(ViewMsg_SpellChecker_Init,
                     IPC::PlatformFileForTransit /* bdict_file */,
                     std::vector<std::string> /* custom_dict_words */,
                     std::string /* language */,
                     bool /* auto spell correct */)

// A word has been added to the custom dictionary; update the local custom
// word list.
IPC_MESSAGE_CONTROL1(ViewMsg_SpellChecker_WordAdded,
                     std::string /* word */)

// Toggle the auto spell correct functionality.
IPC_MESSAGE_CONTROL1(ViewMsg_SpellChecker_EnableAutoSpellCorrect,
                     bool /* enable */)

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

// Enable accessibility in the renderer process.
IPC_MESSAGE_ROUTED0(ViewMsg_EnableAccessibility)

// Relay a request from assistive technology to set focus to a given node.
IPC_MESSAGE_ROUTED1(ViewMsg_SetAccessibilityFocus,
                    int /* object id */)

// Relay a request from assistive technology to perform the default action
// on a given node.
IPC_MESSAGE_ROUTED1(ViewMsg_AccessibilityDoDefaultAction,
                    int /* object id */)

// Tells the render view that a ViewHostMsg_AccessibilityNotifications
// message was processed and it can send addition notifications.
IPC_MESSAGE_ROUTED0(ViewMsg_AccessibilityNotifications_ACK)

// A classification model for client-side phishing detection.
// The given file contains an encoded safe_browsing::ClientSideModel
// protocol buffer.
IPC_MESSAGE_CONTROL1(ViewMsg_SetPhishingModel,
                     IPC::PlatformFileForTransit /* model_file */)

// Request a DOM tree when a malware interstitial is shown.
IPC_MESSAGE_ROUTED0(ViewMsg_GetMalwareDOMDetails)

// Tells the renderer to begin phishing detection for the given toplevel URL
// which it has started loading.
IPC_MESSAGE_ROUTED1(ViewMsg_StartPhishingDetection, GURL)

//-----------------------------------------------------------------------------
// TabContents messages
// These are messages sent from the renderer to the browser process.

IPC_MESSAGE_CONTROL1(ViewHostMsg_UpdatedCacheStats,
                     WebKit::WebCache::UsageStats /* stats */)

// Requests spellcheck for a word.
IPC_SYNC_MESSAGE_ROUTED2_2(ViewHostMsg_SpellCheck,
                           string16 /* word to check */,
                           int /* document tag*/,
                           int /* misspell location */,
                           int /* misspell length */)

// Asks the browser for a unique document tag.
IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_GetDocumentTag,
                           int /* the tag */)

// This message tells the spellchecker that a document, identified by an int
// tag, has been closed and all of the ignored words for that document can be
// forgotten.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentWithTagClosed,
                    int /* the tag */)

// Tells the browser to display or not display the SpellingPanel
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowSpellingPanel,
                    bool /* if true, then show it, otherwise hide it*/)

// Tells the browser to update the spelling panel with the given word.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateSpellingPanelWithMisspelledWord,
                    string16 /* the word to update the panel with */)

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

// Required for updating text input state.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ImeUpdateTextInputState,
                    WebKit::WebTextInputType, /* text_input_type */
                    gfx::Rect /* caret_rect */)

// Required for cancelling an ongoing input method composition.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ImeCancelComposition)

// Tells the browser that the renderer is done calculating the number of
// rendered pages according to the specified settings.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidGetPrintedPagesCount,
                    int /* rendered document cookie */,
                    int /* number of rendered pages */)

// Sends back to the browser the rendered "printed page" that was requested by
// a ViewMsg_PrintPage message or from scripted printing. The memory handle in
// this message is already valid in the browser process.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidPrintPage,
                    ViewHostMsg_DidPrintPage_Params /* page content */)

// The renderer wants to know the default print settings.
IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_GetDefaultPrintSettings,
                           ViewMsg_Print_Params /* default_settings */)

// The renderer wants to update the current print settings with new
// |job_settings|.
IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_UpdatePrintSettings,
                           int /* document_cookie */,
                           DictionaryValue /* job_settings */,
                           ViewMsg_PrintPages_Params /* current_settings */)

// It's the renderer that controls the printing process when it is generated
// by javascript. This step is about showing UI to the user to select the
// final print settings. The output parameter is the same as
// ViewMsg_PrintPages which is executed implicitly.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_ScriptedPrint,
                           ViewHostMsg_ScriptedPrint_Params,
                           ViewMsg_PrintPages_Params
                               /* settings chosen by the user*/)

// WebKit and JavaScript error messages to log to the console
// or debugger UI.
IPC_MESSAGE_ROUTED3(ViewHostMsg_AddMessageToConsole,
                    std::wstring, /* msg */
                    int32, /* line number */
                    std::wstring /* source id */)

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

// Sent by the renderer process to indicate that a plugin instance has
// crashed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_CrashedPlugin,
                    FilePath /* plugin_path */)

// Notifies when a plugin couldn't be loaded because it's outdated.
IPC_MESSAGE_ROUTED2(ViewHostMsg_BlockedOutdatedPlugin,
                    string16, /* name */
                    GURL      /* update_url */)

// Displays a JavaScript out-of-memory message in the infobar.
IPC_MESSAGE_ROUTED0(ViewHostMsg_JSOutOfMemory)

// Displays a box to confirm that the user wants to navigate away from the
// page. Replies true if yes, false otherwise, the reply string is ignored,
// but is included so that we can use OnJavaScriptMessageBoxClosed.
IPC_SYNC_MESSAGE_ROUTED2_2(ViewHostMsg_RunBeforeUnloadConfirm,
                           GURL,        /* in - originating frame URL */
                           std::wstring /* in - alert message */,
                           bool         /* out - success */,
                           std::wstring /* out - This is ignored.*/)

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

// Sent when the renderer process is done processing a DataReceived
// message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DataReceived_ACK,
                    int /* request_id */)

IPC_MESSAGE_CONTROL1(ViewHostMsg_RevealFolderInOS,
                     FilePath /* path */)

// Sent when a provisional load on the main frame redirects.
IPC_MESSAGE_ROUTED3(ViewHostMsg_DidRedirectProvisionalLoad,
                    int /* page_id */,
                    GURL /* last url */,
                    GURL /* url redirected to */)

// Sent when the renderer changes the zoom level for a particular url, so the
// browser can update its records.  If remember is true, then url is used to
// update the zoom level for all pages in that site.  Otherwise, the render
// view's id is used so that only the menu is updated.
IPC_MESSAGE_ROUTED3(ViewHostMsg_DidZoomURL,
                    double /* zoom_level */,
                    bool /* remember */,
                    GURL /* url */)

#if defined(OS_WIN)
// Duplicates a shared memory handle from the renderer to the browser. Then
// the renderer can flush the handle.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_DuplicateSection,
                           base::SharedMemoryHandle /* renderer handle */,
                           base::SharedMemoryHandle /* browser handle */)
#endif

#if defined(USE_X11)
// Asks the browser to create a temporary file for the renderer to fill
// in resulting NativeMetafile in printing.
IPC_SYNC_MESSAGE_CONTROL0_2(ViewHostMsg_AllocateTempFileForPrinting,
                            base::FileDescriptor /* temp file fd */,
                            int /* fd in browser*/)
IPC_MESSAGE_CONTROL1(ViewHostMsg_TempFileForPrintingWritten,
                     int /* fd in browser */)
#endif

// Asks the browser to do print preview for the node under the context menu.
IPC_MESSAGE_ROUTED0(ViewHostMsg_PrintPreviewNodeUnderContextMenu)

// Asks the browser to do print preview for window.print().
IPC_MESSAGE_ROUTED0(ViewHostMsg_ScriptInitiatedPrintPreview)

// Asks the browser to create a block of shared memory for the renderer to
// fill in and pass back to the browser.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_AllocateSharedMemoryBuffer,
                           uint32 /* buffer size */,
                           base::SharedMemoryHandle /* browser handle */)

// Provide the browser process with information about the WebCore resource
// cache.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ResourceTypeStats,
                     WebKit::WebCache::ResourceTypeStats)

// Notify the browser that this render process can or can't be suddenly
// terminated.
IPC_MESSAGE_CONTROL1(ViewHostMsg_SuddenTerminationChanged,
                     bool /* enabled */)

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

#if defined(OS_MACOSX)
// On OSX, we cannot allocated shared memory from within the sandbox, so
// this call exists for the renderer to ask the browser to allocate memory
// on its behalf. We return a file descriptor to the POSIX shared memory.
// If the |cache_in_browser| flag is |true|, then a copy of the shmem is kept
// by the browser, and it is the caller's repsonsibility to send a
// ViewHostMsg_FreeTransportDIB message in order to release the cached shmem.
// In all cases, the caller is responsible for deleting the resulting
// TransportDIB.
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_AllocTransportDIB,
                            size_t, /* bytes requested */
                            bool, /* cache in the browser */
                            TransportDIB::Handle /* DIB */)

// Since the browser keeps handles to the allocated transport DIBs, this
// message is sent to tell the browser that it may release them when the
// renderer is finished with them.
IPC_MESSAGE_CONTROL1(ViewHostMsg_FreeTransportDIB,
                     TransportDIB::Id /* DIB id */)

// Informs the browser that a plugin has gained or lost focus.
IPC_MESSAGE_ROUTED2(ViewHostMsg_PluginFocusChanged,
                    bool, /* focused */
                    int /* plugin_id */)

// Instructs the browser to start plugin IME.
IPC_MESSAGE_ROUTED0(ViewHostMsg_StartPluginIme)

//---------------------------------------------------------------------------
// Messages related to accelerated plugins

// This is sent from the renderer to the browser to allocate a fake
// PluginWindowHandle on the browser side which is used to identify
// the plugin to the browser later when backing store is allocated
// or reallocated. |opaque| indicates whether the plugin's output is
// considered to be opaque, as opposed to translucent. This message
// is reused for rendering the accelerated compositor's output.
// |root| indicates whether the output is supposed to cover the
// entire window.
IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_AllocateFakePluginWindowHandle,
                           bool /* opaque */,
                           bool /* root */,
                           gfx::PluginWindowHandle /* id */)

// Destroys a fake window handle previously allocated using
// AllocateFakePluginWindowHandle.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DestroyFakePluginWindowHandle,
                    gfx::PluginWindowHandle /* id */)

// This message, used on Mac OS X 10.5 and earlier (no IOSurface support),
// is sent from the renderer to the browser on behalf of the plug-in
// to indicate that a new backing store was allocated for that plug-in
// instance.
IPC_MESSAGE_ROUTED4(ViewHostMsg_AcceleratedSurfaceSetTransportDIB,
                    gfx::PluginWindowHandle /* window */,
                    int32 /* width */,
                    int32 /* height */,
                    TransportDIB::Handle /* handle for the DIB */)

// This message, used on Mac OS X 10.6 and later (where IOSurface is
// supported), is sent from the renderer to the browser on behalf of the
// plug-in to indicate that a new backing store was allocated for that
// plug-in instance.
//
// NOTE: the original intent was to pass a mach port as the IOSurface
// identifier but it looks like that will be a lot of work. For now we pass an
// ID from IOSurfaceGetID.
IPC_MESSAGE_ROUTED4(ViewHostMsg_AcceleratedSurfaceSetIOSurface,
                    gfx::PluginWindowHandle /* window */,
                    int32 /* width */,
                    int32 /* height */,
                    uint64 /* surface_id */)

// This message notifies the browser process that the plug-in
// swapped the buffers associated with the given "window", which
// should cause the browser to redraw the various plug-ins'
// contents.
IPC_MESSAGE_ROUTED2(ViewHostMsg_AcceleratedSurfaceBuffersSwapped,
                    gfx::PluginWindowHandle /* window */,
                    uint64 /* surface_id */)
#endif

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

// Sent to notify the browser about renderer accessibility notifications.
// The browser responds with a ViewMsg_AccessibilityNotifications_ACK.
IPC_MESSAGE_ROUTED1(
    ViewHostMsg_AccessibilityNotifications,
    std::vector<ViewHostMsg_AccessibilityNotification_Params>)

// Send part of the DOM to the browser, to be used in a malware report.
IPC_MESSAGE_ROUTED1(ViewHostMsg_MalwareDOMDetails,
                    ViewHostMsg_MalwareDOMDetails_Params)

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

// Opens a file asynchronously. The response returns a file descriptor
// and an error code from base/platform_file.h.
IPC_MESSAGE_ROUTED3(ViewHostMsg_AsyncOpenFile,
                    FilePath /* file path */,
                    int /* flags */,
                    int /* message_id */)

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

//---------------------------------------------------------------------------
// Request for cryptographic operation messages:
// These are messages from the renderer to the browser to perform a
// cryptographic operation.

// Asks the browser process to generate a keypair for grabbing a client
// certificate from a CA (<keygen> tag), and returns the signed public
// key and challenge string.
IPC_SYNC_MESSAGE_CONTROL3_1(ViewHostMsg_Keygen,
                            uint32 /* key size index */,
                            std::string /* challenge string */,
                            GURL /* URL of requestor */,
                            std::string /* signed public key and challenge */)

// The renderer has tried to spell check a word, but couldn't because no
// dictionary was available to load. Request that the browser find an
// appropriate dictionary and return it.
IPC_MESSAGE_CONTROL0(ViewHostMsg_SpellChecker_RequestDictionary)

IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_SpellChecker_PlatformCheckSpelling,
                            string16 /* word */,
                            int /* document tag */,
                            bool /* correct */)

IPC_SYNC_MESSAGE_CONTROL1_1(
    ViewHostMsg_SpellChecker_PlatformFillSuggestionList,
    string16 /* word */,
    std::vector<string16> /* suggestions */)

IPC_MESSAGE_CONTROL4(ViewHostMsg_SpellChecker_PlatformRequestTextCheck,
                     int /* route_id for response */,
                     int /* request identifier given by WebKit */,
                     int /* document tag */,
                     string16 /* sentence */)

// Updates the minimum/maximum allowed zoom percent for this tab from the
// default values.  If |remember| is true, then the zoom setting is applied to
// other pages in the site and is saved, otherwise it only applies to this
// tab.
IPC_MESSAGE_ROUTED3(ViewHostMsg_UpdateZoomLimits,
                    int /* minimum_percent */,
                    int /* maximum_percent */,
                    bool /* remember */)

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
