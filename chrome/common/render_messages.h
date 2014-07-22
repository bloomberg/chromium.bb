// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/search_provider.h"
#include "components/nacl/common/nacl_types.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/referrer.h"
#include "content/public/common/top_controls_state.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/rect.h"

// Singly-included section for enums and custom IPC traits.
#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

// This enum is inside a struct so that we can forward-declare the struct in
// others headers without having to include this one.
struct ChromeViewHostMsg_GetPluginInfo_Status {
  enum Value {
    kAllowed,
    kBlocked,
    kBlockedByPolicy,
    kClickToPlay,
    kDisabled,
    kNotFound,
    kNPAPINotSupported,
    kOutdatedBlocked,
    kOutdatedDisallowed,
    kUnauthorized,
  };

  ChromeViewHostMsg_GetPluginInfo_Status() : value(kAllowed) {}

  Value value;
};

namespace IPC {

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

IPC_ENUM_TRAITS_MAX_VALUE(ChromeViewHostMsg_GetPluginInfo_Status::Value,
                          ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized)
IPC_ENUM_TRAITS(OmniboxFocusChangeReason)
IPC_ENUM_TRAITS(OmniboxFocusState)
IPC_ENUM_TRAITS(search_provider::OSDDType)
IPC_ENUM_TRAITS(search_provider::InstallState)
IPC_ENUM_TRAITS(ThemeBackgroundImageAlignment)
IPC_ENUM_TRAITS(ThemeBackgroundImageTiling)
IPC_ENUM_TRAITS(blink::WebConsoleMessage::Level)
IPC_ENUM_TRAITS(content::TopControlsState)

IPC_STRUCT_TRAITS_BEGIN(ChromeViewHostMsg_GetPluginInfo_Status)
IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

// Output parameters for ChromeViewHostMsg_GetPluginInfo message.
IPC_STRUCT_BEGIN(ChromeViewHostMsg_GetPluginInfo_Output)
  IPC_STRUCT_MEMBER(ChromeViewHostMsg_GetPluginInfo_Status, status)
  IPC_STRUCT_MEMBER(content::WebPluginInfo, plugin)
  IPC_STRUCT_MEMBER(std::string, actual_mime_type)
  IPC_STRUCT_MEMBER(std::string, group_identifier)
  IPC_STRUCT_MEMBER(base::string16, group_name)
IPC_STRUCT_END()

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

IPC_STRUCT_TRAITS_BEGIN(InstantSuggestion)
  IPC_STRUCT_TRAITS_MEMBER(text)
  IPC_STRUCT_TRAITS_MEMBER(metadata)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(InstantMostVisitedItem)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(RendererContentSettingRules)
  IPC_STRUCT_TRAITS_MEMBER(image_rules)
  IPC_STRUCT_TRAITS_MEMBER(script_rules)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(RGBAColor)
  IPC_STRUCT_TRAITS_MEMBER(r)
  IPC_STRUCT_TRAITS_MEMBER(g)
  IPC_STRUCT_TRAITS_MEMBER(b)
  IPC_STRUCT_TRAITS_MEMBER(a)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ThemeBackgroundInfo)
  IPC_STRUCT_TRAITS_MEMBER(using_default_theme)
  IPC_STRUCT_TRAITS_MEMBER(background_color)
  IPC_STRUCT_TRAITS_MEMBER(text_color)
  IPC_STRUCT_TRAITS_MEMBER(link_color)
  IPC_STRUCT_TRAITS_MEMBER(text_color_light)
  IPC_STRUCT_TRAITS_MEMBER(header_color)
  IPC_STRUCT_TRAITS_MEMBER(section_border_color)
  IPC_STRUCT_TRAITS_MEMBER(theme_id)
  IPC_STRUCT_TRAITS_MEMBER(image_horizontal_alignment)
  IPC_STRUCT_TRAITS_MEMBER(image_vertical_alignment)
  IPC_STRUCT_TRAITS_MEMBER(image_tiling)
  IPC_STRUCT_TRAITS_MEMBER(image_height)
  IPC_STRUCT_TRAITS_MEMBER(has_attribution)
  IPC_STRUCT_TRAITS_MEMBER(logo_alternate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebCache::ResourceTypeStat)
  IPC_STRUCT_TRAITS_MEMBER(count)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(liveSize)
  IPC_STRUCT_TRAITS_MEMBER(decodedSize)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebCache::ResourceTypeStats)
  IPC_STRUCT_TRAITS_MEMBER(images)
  IPC_STRUCT_TRAITS_MEMBER(cssStyleSheets)
  IPC_STRUCT_TRAITS_MEMBER(scripts)
  IPC_STRUCT_TRAITS_MEMBER(xslStyleSheets)
  IPC_STRUCT_TRAITS_MEMBER(fonts)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebCache::UsageStats)
  IPC_STRUCT_TRAITS_MEMBER(minDeadCapacity)
  IPC_STRUCT_TRAITS_MEMBER(maxDeadCapacity)
  IPC_STRUCT_TRAITS_MEMBER(capacity)
  IPC_STRUCT_TRAITS_MEMBER(liveSize)
  IPC_STRUCT_TRAITS_MEMBER(deadSize)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(NTPLoggingEventType,
                          NTP_NUM_EVENT_TYPES)

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

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// For WebUI testing, this message requests JavaScript to be executed at a time
// which is late enough to not be thrown out, and early enough to be before
// onload events are fired.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_WebUIJavaScript,
                    base::string16  /* javascript */)
#endif

// Set the content setting rules stored by the renderer.
IPC_MESSAGE_CONTROL1(ChromeViewMsg_SetContentSettingRules,
                     RendererContentSettingRules /* rules */)

// Tells the render frame to load all blocked plugins with the given identifier.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_LoadBlockedPlugins,
                    std::string /* identifier */)

// Asks the renderer to send back stats on the WebCore cache broken down by
// resource types.
IPC_MESSAGE_CONTROL0(ChromeViewMsg_GetCacheResourceStats)

// Tells the renderer to create a FieldTrial, and by using a 100% probability
// for the FieldTrial, forces the FieldTrial to have assigned group name.
IPC_MESSAGE_CONTROL2(ChromeViewMsg_SetFieldTrialGroup,
                     std::string /* field trial name */,
                     std::string /* group name that was assigned. */)

// Asks the renderer to send back V8 heap stats.
IPC_MESSAGE_CONTROL0(ChromeViewMsg_GetV8HeapStats)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetPageSequenceNumber,
                    int /* page_seq_no */)

IPC_MESSAGE_ROUTED0(ChromeViewMsg_DetermineIfPageSupportsInstant)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxSetDisplayInstantResults,
                    bool /* display_instant_results */)

IPC_MESSAGE_ROUTED2(ChromeViewMsg_SearchBoxFocusChanged,
                    OmniboxFocusState /* new_focus_state */,
                    OmniboxFocusChangeReason /* reason */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxMarginChange, int /* start */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxMostVisitedItemsChanged,
                    std::vector<InstantMostVisitedItem> /* items */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxPromoInformation,
                    bool /* is_app_launcher_enabled */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxSetInputInProgress,
                    bool /* input_in_progress */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxSetSuggestionToPrefetch,
                    InstantSuggestion /* suggestion */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxSubmit,
                    base::string16 /* value */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxThemeChanged,
                    ThemeBackgroundInfo /* value */)

IPC_MESSAGE_ROUTED2(ChromeViewMsg_ChromeIdentityCheckResult,
                    base::string16 /* identity */,
                    bool /* identity_match */)

IPC_MESSAGE_ROUTED0(ChromeViewMsg_SearchBoxToggleVoiceSearch)

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

IPC_MESSAGE_ROUTED0(ChromeViewMsg_ReloadFrame)

// Tells the renderer whether or not a file system access has been allowed.
IPC_MESSAGE_ROUTED2(ChromeViewMsg_RequestFileSystemAccessAsyncResponse,
                    int  /* request_id */,
                    bool /* allowed */)

// Sent when the profile changes the kSafeBrowsingEnabled preference.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetClientSidePhishingDetection,
                    bool /* enable_phishing_detection */)

// Asks the renderer for a thumbnail of the image selected by the most
// recently opened context menu, if there is one. If the image's area
// is greater than thumbnail_min_area it will be downscaled to
// be within thumbnail_max_size. The possibly downsampled image will be
// returned in a ChromeViewHostMsg_RequestThumbnailForContextNode_ACK message.
IPC_MESSAGE_ROUTED2(ChromeViewMsg_RequestThumbnailForContextNode,
                    int /* thumbnail_min_area_pixels */,
                    gfx::Size /* thumbnail_max_size_pixels */)

// Notifies the renderer whether hiding/showing the top controls is enabled,
// what the current state should be, and whether or not to animate to the
// proper state.
IPC_MESSAGE_ROUTED3(ChromeViewMsg_UpdateTopControlsState,
                    content::TopControlsState /* constraints */,
                    content::TopControlsState /* current */,
                    bool /* animate */)


// Updates the window features of the render view.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetWindowFeatures,
                    blink::WebWindowFeatures /* window_features */)

IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_RequestThumbnailForContextNode_ACK,
                    SkBitmap /* thumbnail */,
                    gfx::Size /* original size of the image */)

#if defined(OS_ANDROID)
// Asks the renderer to return information about whether the current page can
// be treated as a webapp.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_RetrieveWebappInformation,
                    GURL /* expected_url */)

// Asks the renderer to return information about the given meta tag.
IPC_MESSAGE_ROUTED2(ChromeViewMsg_RetrieveMetaTagContent,
                    GURL /* expected_url */,
                    std::string /* tag_name */ )
#endif  // defined(OS_ANDROID)

// chrome.principals messages ------------------------------------------------

// Message sent from the renderer to the browser to get the list of browser
// managed accounts for the given origin.
IPC_SYNC_MESSAGE_CONTROL1_1(ChromeViewHostMsg_GetManagedAccounts,
                            GURL /* current URL */,
                            std::vector<std::string> /* managed accounts */)

// Message sent from the renderer to the browser to show the browser account
// management UI.
IPC_MESSAGE_CONTROL0(ChromeViewHostMsg_ShowBrowserAccountManagementUI)

// JavaScript related messages -----------------------------------------------

// Tells the frame it is displaying an interstitial page.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_SetAsInterstitial)

// Provides the renderer with the results of the browser's investigation into
// why a recent main frame load failed (currently, just DNS probe result).
// NetErrorHelper will receive this mesage and replace or update the error
// page with more specific troubleshooting suggestions.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_NetErrorInfo,
                    int /* DNS probe status */)

// Provides the information needed by the renderer process to contact a
// navigation correction service.  Handled by the NetErrorHelper.
IPC_MESSAGE_ROUTED5(ChromeViewMsg_SetNavigationCorrectionInfo,
                    GURL /* Navigation correction service base URL */,
                    std::string /* language */,
                    std::string /* origin_country */,
                    std::string /* API key to use */,
                    GURL /* Google Search URL to use */)

//-----------------------------------------------------------------------------
// Misc messages
// These are messages sent from the renderer to the browser process.

IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_UpdatedCacheStats,
                     blink::WebCache::UsageStats /* stats */)

// Tells the browser that content in the current page was blocked due to the
// user's content settings.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_ContentBlocked,
                    ContentSettingsType /* type of blocked content */)

// Sent by the renderer process to check whether access to web databases is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL5_1(ChromeViewHostMsg_AllowDatabase,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            base::string16 /* database name */,
                            base::string16 /* database display name */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to DOM Storage is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_AllowDOMStorage,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            bool /* if true local storage, otherwise session */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to FileSystem is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL3_1(ChromeViewHostMsg_RequestFileSystemAccessSync,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to FileSystem is
// granted by content settings.
IPC_MESSAGE_CONTROL4(ChromeViewHostMsg_RequestFileSystemAccessAsync,
                    int /* render_frame_id */,
                    int /* request_id */,
                    GURL /* origin_url */,
                    GURL /* top origin url */)

// Sent by the renderer process to check whether access to Indexed DBis
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_AllowIndexedDB,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            base::string16 /* database name */,
                            bool /* allowed */)

// Return information about a plugin for the given URL and MIME type.
// In contrast to ViewHostMsg_GetPluginInfo in content/, this IPC call knows
// about specific reasons why a plug-in can't be used, for example because it's
// disabled.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_GetPluginInfo,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* top origin url */,
                            std::string /* mime_type */,
                            ChromeViewHostMsg_GetPluginInfo_Output /* output */)

#if defined(ENABLE_PEPPER_CDMS)
// Returns whether any internal plugin supporting |mime_type| is registered and
// enabled. Does not determine whether the plugin can actually be instantiated
// (e.g. whether it has all its dependencies).
// When the returned *|is_available| is true, |additional_param_names| and
// |additional_param_values| contain the name-value pairs, if any, specified
// for the *first* non-disabled plugin found that is registered for |mime_type|.
IPC_SYNC_MESSAGE_CONTROL1_3(
    ChromeViewHostMsg_IsInternalPluginAvailableForMimeType,
    std::string /* mime_type */,
    bool /* is_available */,
    std::vector<base::string16> /* additional_param_names */,
    std::vector<base::string16> /* additional_param_values */)
#endif

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
                    base::string16 /* plugin_name */)

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

// Tells the browser that there was an error loading a plug-in.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_CouldNotLoadPlugin,
                    base::FilePath /* plugin_path */)

// Tells the browser that we blocked a plug-in because NPAPI is not supported.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_NPAPINotSupported,
                    std::string /* identifer */)

// Tells the renderer that the NPAPI cannot be used. For example Ash on windows.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_NPAPINotSupported)

// Notification that the page has an OpenSearch description document
// associated with it.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_PageHasOSDD,
                    GURL /* page_url */,
                    GURL /* osdd_url */,
                    search_provider::OSDDType)

// Find out if the given url's security origin is installed as a search
// provider.
IPC_SYNC_MESSAGE_ROUTED2_1(ChromeViewHostMsg_GetSearchProviderInstallState,
                           GURL /* page url */,
                           GURL /* inquiry url */,
                           search_provider::InstallState /* install */)

// Sends back stats about the V8 heap.
IPC_MESSAGE_CONTROL2(ChromeViewHostMsg_V8HeapStats,
                     int /* size of heap (allocated from the OS) */,
                     int /* bytes in use */)

// Request for a DNS prefetch of the names in the array.
// NameList is typedef'ed std::vector<std::string>
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_DnsPrefetch,
                     std::vector<std::string> /* hostnames */)

// Request for preconnect to host providing resource specified by URL
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_Preconnect,
                     GURL /* preconnect target url */)

// Notifies when a plugin couldn't be loaded because it's outdated.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_BlockedOutdatedPlugin,
                    int /* placeholder ID */,
                    std::string /* plug-in group identifier */)

// Notifies when a plugin couldn't be loaded because it requires
// user authorization.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_BlockedUnauthorizedPlugin,
                    base::string16 /* name */,
                    std::string /* plug-in group identifier */)

// Provide the browser process with information about the WebCore resource
// cache and current renderer framerate.
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_ResourceTypeStats,
                     blink::WebCache::ResourceTypeStats)

// Message sent from the renderer to the browser to notify it of a
// window.print() call which should cancel the prerender. The message is sent
// only when the renderer is prerendering.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_CancelPrerenderForPrinting)

// Sent when the renderer was prevented from displaying insecure content in
// a secure page by a security policy.  The page may appear incomplete.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_DidBlockDisplayingInsecureContent)

// Sent when the renderer was prevented from running insecure content in
// a secure origin by a security policy.  The page may appear incomplete.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_DidBlockRunningInsecureContent)

#if defined(OS_ANDROID)
// Contains info about whether the current page can be treated as a webapp.
IPC_MESSAGE_ROUTED4(ChromeViewHostMsg_DidRetrieveWebappInformation,
                    bool /* success */,
                    bool /* is_mobile_webapp_capable */,
                    bool /* is_apple_mobile_webapp_capable */,
                    GURL /* expected_url */)

IPC_MESSAGE_ROUTED4(ChromeViewHostMsg_DidRetrieveMetaTagContent,
                    bool /* success */,
                    std::string /* tag_name */,
                    std::string /* tag_content */,
                    GURL /* expected_url */)
#endif  // defined(OS_ANDROID)

// The currently displayed PDF has an unsupported feature.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_PDFHasUnsupportedFeature)

// Brings up SaveAs... dialog to save specified URL.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_PDFSaveURLAs,
                    GURL /* url */,
                    content::Referrer /* referrer */)

// Updates the content restrictions, i.e. to disable print/copy.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_PDFUpdateContentRestrictions,
                    int /* restrictions */)

// Brings up a Password... dialog for protected documents.
IPC_SYNC_MESSAGE_ROUTED1_1(ChromeViewHostMsg_PDFModalPromptForPassword,
                           std::string /* prompt */,
                           std::string /* actual_value */)

// This message indicates the error appeared in the frame.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_FrameLoadingError,
                    int /* error */)

// This message indicates the monitored frame loading had completed.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_FrameLoadingCompleted)

// Logs events from InstantExtended New Tab Pages.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_LogEvent,
                    int /* page_seq_no */,
                    NTPLoggingEventType /* event */)

// Logs an impression on one of the Most Visited tile on the InstantExtended
// New Tab Page.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_LogMostVisitedImpression,
                    int /* page_seq_no */,
                    int /* position */,
                    base::string16 /* provider */)

// Logs a navigation on one of the Most Visited tile on the InstantExtended
// New Tab Page.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_LogMostVisitedNavigation,
                    int /* page_seq_no */,
                    int /* position */,
                    base::string16 /* provider */)

// The Instant page asks for Chrome identity check against |identity|.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_ChromeIdentityCheck,
                    int /* page_seq_no */,
                    base::string16 /* identity */)

// Tells InstantExtended to set the omnibox focus state.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_FocusOmnibox,
                    int /* page_seq_no */,
                    OmniboxFocusState /* state */)

// Tells InstantExtended to paste text into the omnibox.  If text is empty,
// the clipboard contents will be pasted. This causes the omnibox dropdown to
// open.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_PasteAndOpenDropdown,
                    int /* page_seq_no */,
                    base::string16 /* text to be pasted */)

// Tells InstantExtended whether the embedded search API is supported.
// See http://dev.chromium.org/embeddedsearch
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_InstantSupportDetermined,
                    int /* page_seq_no */,
                    bool /* result */)

// Tells InstantExtended to delete a most visited item.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem,
                    int /* page_seq_no */,
                    GURL /* url */)

// Tells InstantExtended to navigate the active tab to a possibly privileged
// URL.
IPC_MESSAGE_ROUTED4(ChromeViewHostMsg_SearchBoxNavigate,
                    int /* page_seq_no */,
                    GURL /* destination */,
                    WindowOpenDisposition /* disposition */,
                    bool /*is_most_visited_item_url*/)

// Tells InstantExtended to undo all most visited item deletions.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions,
                    int /* page_seq_no */)

// Tells InstantExtended to undo one most visited item deletion.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion,
                    int /* page_seq_no */,
                    GURL /* url */)

// Tells InstantExtended whether the page supports voice search.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_SetVoiceSearchSupported,
                    int /* page_seq_no */,
                    bool /* supported */)

// Tells the renderer a list of URLs which should be bounced back to the browser
// process so that they can be assigned to an Instant renderer.
IPC_MESSAGE_CONTROL2(ChromeViewMsg_SetSearchURLs,
                     std::vector<GURL> /* search_urls */,
                     GURL /* new_tab_page_url */)

#if defined(ENABLE_PLUGINS)
// Sent by the renderer to check if crash reporting is enabled.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromeViewHostMsg_IsCrashReportingEnabled,
                            bool /* enabled */)
#endif
