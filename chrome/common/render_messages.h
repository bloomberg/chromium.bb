// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <stdint.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/features.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/web_application_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/common/browser_controls_state.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/features/features.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"
#include "url/origin.h"

// Singly-included section for enums and custom IPC traits.
#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

// These are only used internally, so the order does not matter.
enum class ChromeViewHostMsg_GetPluginInfo_Status {
  kAllowed,
  // Plugin is blocked, but still can be manually loaded via context menu.
  kBlocked,
  // Plugin is blocked by policy, so it cannot be manually loaded.
  kBlockedByPolicy,
  // Plugin is blocked, and cannot be manually loaded via context menu.
  kBlockedNoLoading,
  kComponentUpdateRequired,
  kDisabled,
  // Flash is blocked, but user can click on the placeholder to trigger the
  // Flash permission prompt.
  kFlashHiddenPreferHtml,
  kNotFound,
  kOutdatedBlocked,
  kOutdatedDisallowed,
  kPlayImportantContent,
  kRestartRequired,
  kUnauthorized,
};

namespace IPC {

template <>
struct ParamTraits<ContentSettingsPattern> {
  typedef ContentSettingsPattern param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_

#define IPC_MESSAGE_START ChromeMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(ChromeViewHostMsg_GetPluginInfo_Status,
                          ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized)
IPC_ENUM_TRAITS_MAX_VALUE(OmniboxFocusChangeReason,
                          OMNIBOX_FOCUS_CHANGE_REASON_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(OmniboxFocusState, OMNIBOX_FOCUS_STATE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ThemeBackgroundImageAlignment,
                          THEME_BKGRND_IMAGE_ALIGN_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ThemeBackgroundImageTiling, THEME_BKGRND_IMAGE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebConsoleMessage::Level,
                          blink::WebConsoleMessage::LevelLast)
IPC_ENUM_TRAITS_MAX_VALUE(content::BrowserControlsState,
                          content::BROWSER_CONTROLS_STATE_LAST)

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

IPC_STRUCT_TRAITS_BEGIN(EmbeddedSearchRequestParams)
  IPC_STRUCT_TRAITS_MEMBER(search_query)
  IPC_STRUCT_TRAITS_MEMBER(original_query)
  IPC_STRUCT_TRAITS_MEMBER(rlz_parameter_value)
  IPC_STRUCT_TRAITS_MEMBER(input_encoding)
  IPC_STRUCT_TRAITS_MEMBER(assisted_query_stats)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(InstantSuggestion)
  IPC_STRUCT_TRAITS_MEMBER(text)
  IPC_STRUCT_TRAITS_MEMBER(metadata)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(InstantMostVisitedItem)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(thumbnail)
  IPC_STRUCT_TRAITS_MEMBER(favicon)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(RendererContentSettingRules)
  IPC_STRUCT_TRAITS_MEMBER(image_rules)
  IPC_STRUCT_TRAITS_MEMBER(script_rules)
  IPC_STRUCT_TRAITS_MEMBER(autoplay_rules)
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

IPC_ENUM_TRAITS_MAX_VALUE(NTPLoggingEventType,
                          NTP_EVENT_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(NTPLoggingTileSource,
                          NTPLoggingTileSource::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(WebApplicationInfo::MobileCapable,
                          WebApplicationInfo::MOBILE_CAPABLE_APPLE)

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
  IPC_STRUCT_TRAITS_MEMBER(mobile_capable)
IPC_STRUCT_TRAITS_END()

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

#if !defined(OS_ANDROID)
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

// Tells the renderer to create a FieldTrial, and by using a 100% probability
// for the FieldTrial, forces the FieldTrial to have assigned group name.
IPC_MESSAGE_CONTROL2(ChromeViewMsg_SetFieldTrialGroup,
                     std::string /* field trial name */,
                     std::string /* group name that was assigned. */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetPageSequenceNumber,
                    int /* page_seq_no */)

IPC_MESSAGE_ROUTED0(ChromeViewMsg_DetermineIfPageSupportsInstant)

IPC_MESSAGE_ROUTED2(ChromeViewMsg_SearchBoxFocusChanged,
                    OmniboxFocusState /* new_focus_state */,
                    OmniboxFocusChangeReason /* reason */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxMostVisitedItemsChanged,
                    std::vector<InstantMostVisitedItem> /* items */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxSetInputInProgress,
                    bool /* input_in_progress */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxSetSuggestionToPrefetch,
                    InstantSuggestion /* suggestion */)

IPC_MESSAGE_ROUTED2(ChromeViewMsg_SearchBoxSubmit,
                    base::string16 /* value */,
                    EmbeddedSearchRequestParams /* params */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_SearchBoxThemeChanged,
                    ThemeBackgroundInfo /* value */)

IPC_MESSAGE_ROUTED1(ChromeViewMsg_HistorySyncCheckResult,
                    bool /* sync_history */)

IPC_MESSAGE_ROUTED2(ChromeViewMsg_ChromeIdentityCheckResult,
                    base::string16 /* identity */,
                    bool /* identity_match */)

// Sent on process startup to indicate whether this process is running in
// incognito mode.
IPC_MESSAGE_CONTROL1(ChromeViewMsg_SetIsIncognitoProcess,
                     bool /* is_incognito_processs */)

// Sent in response to FrameHostMsg_DidBlockRunningInsecureContent.
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

// Reloads the image selected by the most recently opened context menu
// (if there indeed is an image at that location).
IPC_MESSAGE_ROUTED0(ChromeViewMsg_RequestReloadImageForContextNode)

// Asks the renderer for a thumbnail of the image selected by the most
// recently opened context menu, if there is one. If the image's area
// is greater than thumbnail_min_area it will be downscaled to
// be within thumbnail_max_size. The possibly downsampled image will be
// returned in a ChromeViewHostMsg_RequestThumbnailForContextNode_ACK message.
IPC_MESSAGE_ROUTED3(ChromeViewMsg_RequestThumbnailForContextNode,
                    int /* thumbnail_min_area_pixels */,
                    gfx::Size /* thumbnail_max_size_pixels */,
                    int /* ID of the callback */)

// Notifies the renderer whether hiding/showing the browser controls is enabled,
// what the current state should be, and whether or not to animate to the
// proper state.
IPC_MESSAGE_ROUTED3(ChromeViewMsg_UpdateBrowserControlsState,
                    content::BrowserControlsState /* constraints */,
                    content::BrowserControlsState /* current */,
                    bool /* animate */)

// Updates the window features of the render view.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetWindowFeatures,
                    blink::WebWindowFeatures /* window_features */)

// Responds to the request for a thumbnail.
// Thumbnail data will be empty is a thumbnail could not be produced.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_RequestThumbnailForContextNode_ACK,
                    std::string /* JPEG-encoded thumbnail data */,
                    gfx::Size /* original size of the image */,
                    int /* ID of the callback */)

// Requests application info for the page. The renderer responds back with
// ChromeViewHostMsg_DidGetWebApplicationInfo.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_GetWebApplicationInfo)

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

#if defined(OS_ANDROID)
// Message sent from the renderer to the browser to schedule to download the
// page at a later time.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_DownloadPageLater)
#endif  // defined(OS_ANDROID)

//-----------------------------------------------------------------------------
// Misc messages
// These are messages sent from the renderer to the browser process.

IPC_MESSAGE_CONTROL2(ChromeViewHostMsg_UpdatedCacheStats,
                     uint64_t /* capacity */,
                     uint64_t /* size */)

// Tells the browser that content in the current page was blocked due to the
// user's content settings.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_ContentBlocked,
                    ContentSettingsType /* type of blocked content */,
                    base::string16 /* details on blocked content */)

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

// Sent by the renderer process when a keygen element is rendered onto the
// current page.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_DidUseKeygen,
                    GURL /* origin_url */)

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
// about specific reasons why a plugin can't be used, for example because it's
// disabled.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_GetPluginInfo,
                            int /* render_frame_id */,
                            GURL /* url */,
                            url::Origin /* main_frame_origin */,
                            std::string /* mime_type */,
                            ChromeViewHostMsg_GetPluginInfo_Output /* output */)

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
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

#if BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)
// Notifies the browser that a missing plugin placeholder has been removed, so
// the corresponding PluginPlaceholderHost can be deleted.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_RemovePluginPlaceholderHost,
                    int /* placeholder_id */)

// Notifies a missing plugin placeholder that a plugin with name |plugin_name|
// has been found.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_FoundMissingPlugin,
                    base::string16 /* plugin_name */)

// Notifies a missing plugin placeholder that no plugin has been found.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_DidNotFindMissingPlugin)

// Notifies a missing plugin placeholder that we have started downloading
// the plugin.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_StartedDownloadingPlugin)

// Notifies a missing plugin placeholder that we have finished downloading
// the plugin.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_FinishedDownloadingPlugin)

// Notifies a missing plugin placeholder that there was an error downloading
// the plugin.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_ErrorDownloadingPlugin,
                    std::string /* message */)
#endif  // BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)

// Notifies a missing plugin placeholder that we have finished component-
// updating the plug-in.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_PluginComponentUpdateSuccess)

// Notifies a missing plugin placeholder that we have failed to component-update
// the plug-in.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_PluginComponentUpdateFailure)

// Notifies a missing plugin placeholder that we have started the component
// download.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_PluginComponentUpdateDownloading)

// Notifies a missing plugin placeholder that the user cancelled downloading
// the plugin.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_CancelledDownloadingPlugin)

// Tells the browser to open chrome://plugins in a new tab. We use a separate
// message because renderer processes aren't allowed to directly navigate to
// chrome:// URLs.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_OpenAboutPlugins)

// Tells the browser to show the Flash permission bubble in the same tab.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_ShowFlashPermissionBubble)

// Tells the browser that there was an error loading a plugin.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_CouldNotLoadPlugin,
                    base::FilePath /* plugin_path */)

// Notification that the page has an OpenSearch description document
// associated with it.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_PageHasOSDD,
                    GURL /* page_url */,
                    GURL /* osdd_url */)

// Notifies when a plugin couldn't be loaded because it's outdated.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_BlockedOutdatedPlugin,
                    int /* placeholder ID */,
                    std::string /* plugin group identifier */)

// Notifies when a plugin couldn't be loaded because it requires a component
// update.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_BlockedComponentUpdatedPlugin,
                    int /* placeholder ID */,
                    std::string /* plugin group identifier */)

// Notifies when a plugin couldn't be loaded because it requires
// user authorization.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_BlockedUnauthorizedPlugin,
                    base::string16 /* name */,
                    std::string /* plugin group identifier */)

// Message sent from the renderer to the browser to notify it of a
// window.print() call which should cancel the prerender. The message is sent
// only when the renderer is prerendering.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_CancelPrerenderForPrinting)

// Sent when the renderer was prevented from displaying insecure content in
// a secure page by a security policy.  The page may appear incomplete.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_DidBlockDisplayingInsecureContent)

IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_DidGetWebApplicationInfo,
                    WebApplicationInfo)

// Logs events from InstantExtended New Tab Pages.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_LogEvent,
                    int /* page_seq_no */,
                    NTPLoggingEventType /* event */,
                    base::TimeDelta /* time */)

// Logs an impression on one of the Most Visited tile on the InstantExtended
// New Tab Page.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_LogMostVisitedImpression,
                    int /* page_seq_no */,
                    int /* position */,
                    NTPLoggingTileSource /* tile_source */)

// Logs a navigation on one of the Most Visited tile on the InstantExtended
// New Tab Page.
IPC_MESSAGE_ROUTED3(ChromeViewHostMsg_LogMostVisitedNavigation,
                    int /* page_seq_no */,
                    int /* position */,
                    NTPLoggingTileSource /* tile_source */)

// The Instant page asks whether the user syncs its history.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_HistorySyncCheck,
                    int /* page_seq_no */)

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

// Tells InstantExtended to undo all most visited item deletions.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions,
                    int /* page_seq_no */)

// Tells InstantExtended to undo one most visited item deletion.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion,
                    int /* page_seq_no */,
                    GURL /* url */)

// Tells the renderer a list of URLs which should be bounced back to the browser
// process so that they can be assigned to an Instant renderer.
IPC_MESSAGE_CONTROL2(ChromeViewMsg_SetSearchURLs,
                     std::vector<GURL> /* search_urls */,
                     GURL /* new_tab_page_url */)

#if BUILDFLAG(ENABLE_PLUGINS)
// Sent by the renderer to check if crash reporting is enabled.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromeViewHostMsg_IsCrashReportingEnabled,
                            bool /* enabled */)
#endif

// Sent by the renderer to indicate that a fields trial has been activated.
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_FieldTrialActivated,
                     std::string /* name */)
