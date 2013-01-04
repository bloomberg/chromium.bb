// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_registry.h"

#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/extensions/api/app/app_api.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/api/cloud_print_private/cloud_print_private_api.h"
#include "chrome/browser/extensions/api/commands/commands.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"
#include "chrome/browser/extensions/api/context_menu/context_menu_api.h"
#include "chrome/browser/extensions/api/cookies/cookies_api.h"
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/browser/extensions/api/declarative/declarative_api.h"
#include "chrome/browser/extensions/api/extension_action/extension_browser_actions_api.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api.h"
#include "chrome/browser/extensions/api/extension_action/extension_script_badge_api.h"
#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"
#include "chrome/browser/extensions/api/history/history_api.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/i18n/i18n_api.h"
#include "chrome/browser/extensions/api/idle/idle_api.h"
#include "chrome/browser/extensions/api/managed_mode/managed_mode_api.h"
#include "chrome/browser/extensions/api/management/management_api.h"
#include "chrome/browser/extensions/api/metrics/metrics.h"
#include "chrome/browser/extensions/api/module/module.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/api/record/record_api.h"
#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/api/serial/serial_api.h"
#include "chrome/browser/extensions/api/socket/socket_api.h"
#include "chrome/browser/extensions/api/tabs/execute_code_in_tab_function.h"
#include "chrome/browser/extensions/api/tabs/tabs.h"
#include "chrome/browser/extensions/api/test/test_api.h"
#include "chrome/browser/extensions/api/top_sites/top_sites_api.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/api/web_socket_proxy_private/web_socket_proxy_private_api.h"
#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"
#include "chrome/browser/extensions/settings/settings_api.h"
#include "chrome/browser/extensions/system/system_api.h"
#include "chrome/browser/infobars/infobar_extension_api.h"
#include "chrome/browser/rlz/rlz_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/speech_input_extension_api.h"
#include "chrome/common/extensions/api/generated_api.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/api/input/input.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/echo_private_api.h"
#include "chrome/browser/chromeos/extensions/file_browser_handler_api.h"
#include "chrome/browser/chromeos/extensions/info_private_api.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/chromeos/extensions/power/power_api.h"
#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"
#include "chrome/browser/chromeos/media/media_player_extension_api.h"
#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"
#endif

// static
ExtensionFunctionRegistry* ExtensionFunctionRegistry::GetInstance() {
  return Singleton<ExtensionFunctionRegistry>::get();
}

ExtensionFunctionRegistry::ExtensionFunctionRegistry() {
  ResetFunctions();
}

ExtensionFunctionRegistry::~ExtensionFunctionRegistry() {
}

void ExtensionFunctionRegistry::ResetFunctions() {
#if defined(ENABLE_EXTENSIONS)

  // Register all functions here.

  // Windows
  RegisterFunction<GetWindowFunction>();
  RegisterFunction<GetCurrentWindowFunction>();
  RegisterFunction<GetLastFocusedWindowFunction>();
  RegisterFunction<GetAllWindowsFunction>();
  RegisterFunction<CreateWindowFunction>();
  RegisterFunction<UpdateWindowFunction>();
  RegisterFunction<RemoveWindowFunction>();

  // Tabs
  RegisterFunction<CaptureVisibleTabFunction>();
  RegisterFunction<CreateTabFunction>();
  RegisterFunction<DetectTabLanguageFunction>();
  RegisterFunction<DuplicateTabFunction>();
  RegisterFunction<GetAllTabsInWindowFunction>();
  RegisterFunction<GetCurrentTabFunction>();
  RegisterFunction<GetSelectedTabFunction>();
  RegisterFunction<GetTabFunction>();
  RegisterFunction<HighlightTabsFunction>();
  RegisterFunction<MoveTabsFunction>();
  RegisterFunction<QueryTabsFunction>();
  RegisterFunction<ReloadTabFunction>();
  RegisterFunction<RemoveTabsFunction>();
  RegisterFunction<TabsExecuteScriptFunction>();
  RegisterFunction<TabsInsertCSSFunction>();
  RegisterFunction<UpdateTabFunction>();

  // Page Actions.
  RegisterFunction<EnablePageActionsFunction>();
  RegisterFunction<DisablePageActionsFunction>();
  RegisterFunction<PageActionShowFunction>();
  RegisterFunction<PageActionHideFunction>();
  RegisterFunction<PageActionSetIconFunction>();
  RegisterFunction<PageActionSetTitleFunction>();
  RegisterFunction<PageActionSetPopupFunction>();
  RegisterFunction<PageActionGetTitleFunction>();
  RegisterFunction<PageActionGetPopupFunction>();

  // Browser Actions.
  RegisterFunction<BrowserActionSetIconFunction>();
  RegisterFunction<BrowserActionSetTitleFunction>();
  RegisterFunction<BrowserActionSetBadgeTextFunction>();
  RegisterFunction<BrowserActionSetBadgeBackgroundColorFunction>();
  RegisterFunction<BrowserActionSetPopupFunction>();
  RegisterFunction<BrowserActionGetTitleFunction>();
  RegisterFunction<BrowserActionGetBadgeTextFunction>();
  RegisterFunction<BrowserActionGetBadgeBackgroundColorFunction>();
  RegisterFunction<BrowserActionGetPopupFunction>();
  RegisterFunction<BrowserActionEnableFunction>();
  RegisterFunction<BrowserActionDisableFunction>();

  // Script Badges.
  RegisterFunction<ScriptBadgeGetAttentionFunction>();
  RegisterFunction<ScriptBadgeGetPopupFunction>();
  RegisterFunction<ScriptBadgeSetPopupFunction>();

  // Browsing Data.
  RegisterFunction<RemoveBrowsingDataFunction>();
  RegisterFunction<RemoveAppCacheFunction>();
  RegisterFunction<RemoveCacheFunction>();
  RegisterFunction<RemoveCookiesFunction>();
  RegisterFunction<RemoveDownloadsFunction>();
  RegisterFunction<RemoveFileSystemsFunction>();
  RegisterFunction<RemoveFormDataFunction>();
  RegisterFunction<RemoveHistoryFunction>();
  RegisterFunction<RemoveIndexedDBFunction>();
  RegisterFunction<RemoveLocalStorageFunction>();
  RegisterFunction<RemovePluginDataFunction>();
  RegisterFunction<RemovePasswordsFunction>();
  RegisterFunction<RemoveWebSQLFunction>();

  // Bookmarks.
  RegisterFunction<extensions::GetBookmarksFunction>();
  RegisterFunction<extensions::GetBookmarkChildrenFunction>();
  RegisterFunction<extensions::GetBookmarkRecentFunction>();
  RegisterFunction<extensions::GetBookmarkTreeFunction>();
  RegisterFunction<extensions::GetBookmarkSubTreeFunction>();
  RegisterFunction<extensions::ImportBookmarksFunction>();
  RegisterFunction<extensions::ExportBookmarksFunction>();
  RegisterFunction<extensions::SearchBookmarksFunction>();
  RegisterFunction<extensions::RemoveBookmarkFunction>();
  RegisterFunction<extensions::RemoveTreeBookmarkFunction>();
  RegisterFunction<extensions::CreateBookmarkFunction>();
  RegisterFunction<extensions::MoveBookmarkFunction>();
  RegisterFunction<extensions::UpdateBookmarkFunction>();

  // Infobars.
  RegisterFunction<ShowInfoBarFunction>();

  // BookmarkManager
  RegisterFunction<extensions::CopyBookmarkManagerFunction>();
  RegisterFunction<extensions::CutBookmarkManagerFunction>();
  RegisterFunction<extensions::PasteBookmarkManagerFunction>();
  RegisterFunction<extensions::CanPasteBookmarkManagerFunction>();
  RegisterFunction<extensions::SortChildrenBookmarkManagerFunction>();
  RegisterFunction<extensions::BookmarkManagerGetStringsFunction>();
  RegisterFunction<extensions::StartDragBookmarkManagerFunction>();
  RegisterFunction<extensions::DropBookmarkManagerFunction>();
  RegisterFunction<extensions::GetSubtreeBookmarkManagerFunction>();
  RegisterFunction<extensions::CanEditBookmarkManagerFunction>();
  RegisterFunction<extensions::CanOpenNewWindowsBookmarkFunction>();

  // History
  RegisterFunction<extensions::AddUrlHistoryFunction>();
  RegisterFunction<extensions::DeleteAllHistoryFunction>();
  RegisterFunction<extensions::DeleteRangeHistoryFunction>();
  RegisterFunction<extensions::DeleteUrlHistoryFunction>();
  RegisterFunction<extensions::GetMostVisitedHistoryFunction>();
  RegisterFunction<extensions::GetVisitsHistoryFunction>();
  RegisterFunction<extensions::SearchHistoryFunction>();

  // Idle
  RegisterFunction<extensions::IdleQueryStateFunction>();
  RegisterFunction<extensions::IdleSetDetectionIntervalFunction>();

  // I18N.
  RegisterFunction<GetAcceptLanguagesFunction>();

  // Metrics.
  RegisterFunction<extensions::MetricsRecordUserActionFunction>();
  RegisterFunction<extensions::MetricsRecordValueFunction>();
  RegisterFunction<extensions::MetricsRecordPercentageFunction>();
  RegisterFunction<extensions::MetricsRecordCountFunction>();
  RegisterFunction<extensions::MetricsRecordSmallCountFunction>();
  RegisterFunction<extensions::MetricsRecordMediumCountFunction>();
  RegisterFunction<extensions::MetricsRecordTimeFunction>();
  RegisterFunction<extensions::MetricsRecordMediumTimeFunction>();
  RegisterFunction<extensions::MetricsRecordLongTimeFunction>();

  // RLZ (not supported on ChromeOS yet).
#if defined(ENABLE_RLZ) && !defined(OS_CHROMEOS)
  RegisterFunction<RlzRecordProductEventFunction>();
  RegisterFunction<RlzGetAccessPointRlzFunction>();
  RegisterFunction<RlzSendFinancialPingFunction>();
  RegisterFunction<RlzClearProductStateFunction>();
#endif

  // Cookies.
  RegisterFunction<extensions::GetCookieFunction>();
  RegisterFunction<extensions::GetAllCookiesFunction>();
  RegisterFunction<extensions::SetCookieFunction>();
  RegisterFunction<extensions::RemoveCookieFunction>();
  RegisterFunction<extensions::GetAllCookieStoresFunction>();

  // Test.
  RegisterFunction<extensions::TestNotifyPassFunction>();
  RegisterFunction<extensions::TestFailFunction>();
  RegisterFunction<extensions::TestLogFunction>();
  RegisterFunction<extensions::TestResetQuotaFunction>();
  RegisterFunction<extensions::TestCreateIncognitoTabFunction>();
  RegisterFunction<extensions::TestSendMessageFunction>();
  RegisterFunction<extensions::TestGetConfigFunction>();

  // Record.
  RegisterFunction<extensions::CaptureURLsFunction>();
  RegisterFunction<extensions::ReplayURLsFunction>();

  // Accessibility.
  RegisterFunction<GetFocusedControlFunction>();
  RegisterFunction<SetAccessibilityEnabledFunction>();
  RegisterFunction<GetAlertsForTabFunction>();

  // Text-to-speech.
  RegisterFunction<ExtensionTtsEngineSendTtsEventFunction>();
  RegisterFunction<ExtensionTtsGetVoicesFunction>();
  RegisterFunction<ExtensionTtsIsSpeakingFunction>();
  RegisterFunction<ExtensionTtsSpeakFunction>();
  RegisterFunction<ExtensionTtsStopSpeakingFunction>();

  // Commands.
  RegisterFunction<GetAllCommandsFunction>();

  // Context Menus.
  RegisterFunction<extensions::CreateContextMenuFunction>();
  RegisterFunction<extensions::UpdateContextMenuFunction>();
  RegisterFunction<extensions::RemoveContextMenuFunction>();
  RegisterFunction<extensions::RemoveAllContextMenusFunction>();

  // Omnibox.
  RegisterFunction<extensions::OmniboxSendSuggestionsFunction>();
  RegisterFunction<extensions::OmniboxSetDefaultSuggestionFunction>();

#if defined(ENABLE_INPUT_SPEECH)
  // Speech input.
  RegisterFunction<StartSpeechInputFunction>();
  RegisterFunction<StopSpeechInputFunction>();
  RegisterFunction<IsRecordingSpeechInputFunction>();
#endif

#if defined(TOOLKIT_VIEWS)
  // Input.
  RegisterFunction<extensions::SendKeyboardEventInputFunction>();
#endif

#if defined(OS_CHROMEOS)
  // Power
  RegisterFunction<extensions::power::RequestKeepAwakeFunction>();
  RegisterFunction<extensions::power::ReleaseKeepAwakeFunction>();
#endif

  // Managed mode.
  RegisterFunction<extensions::GetManagedModeFunction>();
  RegisterFunction<extensions::EnterManagedModeFunction>();
  RegisterFunction<extensions::GetPolicyFunction>();
  RegisterFunction<extensions::SetPolicyFunction>();

  // Management.
  RegisterFunction<GetAllExtensionsFunction>();
  RegisterFunction<GetExtensionByIdFunction>();
  RegisterFunction<GetPermissionWarningsByIdFunction>();
  RegisterFunction<GetPermissionWarningsByManifestFunction>();
  RegisterFunction<LaunchAppFunction>();
  RegisterFunction<SetEnabledFunction>();
  RegisterFunction<UninstallFunction>();

  // Extension module.
  RegisterFunction<extensions::SetUpdateUrlDataFunction>();
  RegisterFunction<extensions::IsAllowedIncognitoAccessFunction>();
  RegisterFunction<extensions::IsAllowedFileSchemeAccessFunction>();

  // WebstorePrivate.
  RegisterFunction<extensions::GetBrowserLoginFunction>();
  RegisterFunction<extensions::GetStoreLoginFunction>();
  RegisterFunction<extensions::SetStoreLoginFunction>();
  RegisterFunction<extensions::InstallBundleFunction>();
  RegisterFunction<extensions::BeginInstallWithManifestFunction>();
  RegisterFunction<extensions::CompleteInstallFunction>();
  RegisterFunction<extensions::GetWebGLStatusFunction>();

  // WebNavigation.
  RegisterFunction<extensions::GetFrameFunction>();
  RegisterFunction<extensions::GetAllFramesFunction>();

  // WebRequest.
  RegisterFunction<WebRequestAddEventListener>();
  RegisterFunction<WebRequestEventHandled>();
  RegisterFunction<WebRequestHandlerBehaviorChanged>();

  // Preferences.
  RegisterFunction<extensions::GetPreferenceFunction>();
  RegisterFunction<extensions::SetPreferenceFunction>();
  RegisterFunction<extensions::ClearPreferenceFunction>();

  // ChromeOS-specific part of the API.
#if defined(OS_CHROMEOS)
  // Device Customization.
  RegisterFunction<extensions::GetChromeosInfoFunction>();

  // FileBrowserHandlerInternal.
  RegisterFunction<FileHandlerSelectFileFunction>();

  // Mediaplayer
  RegisterFunction<PlayMediaplayerFunction>();
  RegisterFunction<GetPlaylistMediaplayerFunction>();
  RegisterFunction<SetWindowHeightMediaplayerFunction>();
  RegisterFunction<CloseWindowMediaplayerFunction>();

  // WallpaperManagerPrivate functions.
  RegisterFunction<WallpaperStringsFunction>();
  RegisterFunction<WallpaperSetWallpaperIfExistFunction>();
  RegisterFunction<WallpaperSetWallpaperFunction>();
  RegisterFunction<WallpaperSetCustomWallpaperFunction>();
  RegisterFunction<WallpaperMinimizeInactiveWindowsFunction>();
  RegisterFunction<WallpaperRestoreMinimizedWindowsFunction>();
  RegisterFunction<WallpaperGetThumbnailFunction>();
  RegisterFunction<WallpaperSaveThumbnailFunction>();
  RegisterFunction<WallpaperGetOfflineWallpaperListFunction>();

  // InputMethod
  RegisterFunction<extensions::GetInputMethodFunction>();

  // Echo
  RegisterFunction<GetRegistrationCodeFunction>();

  // Terminal
  RegisterFunction<OpenTerminalProcessFunction>();
  RegisterFunction<SendInputToTerminalProcessFunction>();
  RegisterFunction<CloseTerminalProcessFunction>();
  RegisterFunction<OnTerminalResizeFunction>();
#endif

  // Websocket to TCP proxy. Currently noop on anything other than ChromeOS.
  RegisterFunction<
      extensions::WebSocketProxyPrivateGetPassportForTCPFunction>();
  RegisterFunction<extensions::WebSocketProxyPrivateGetURLForTCPFunction>();

  // Debugger
  RegisterFunction<AttachDebuggerFunction>();
  RegisterFunction<DetachDebuggerFunction>();
  RegisterFunction<SendCommandDebuggerFunction>();

  // Settings
  RegisterFunction<extensions::GetSettingsFunction>();
  RegisterFunction<extensions::SetSettingsFunction>();
  RegisterFunction<extensions::RemoveSettingsFunction>();
  RegisterFunction<extensions::ClearSettingsFunction>();
  RegisterFunction<extensions::GetBytesInUseSettingsFunction>();

  // Content settings.
  RegisterFunction<extensions::GetResourceIdentifiersFunction>();
  RegisterFunction<extensions::ClearContentSettingsFunction>();
  RegisterFunction<extensions::GetContentSettingFunction>();
  RegisterFunction<extensions::SetContentSettingFunction>();

  // Font settings.
  RegisterFunction<extensions::GetFontListFunction>();
  RegisterFunction<extensions::ClearFontFunction>();
  RegisterFunction<extensions::GetFontFunction>();
  RegisterFunction<extensions::SetFontFunction>();
  RegisterFunction<extensions::ClearDefaultFontSizeFunction>();
  RegisterFunction<extensions::GetDefaultFontSizeFunction>();
  RegisterFunction<extensions::SetDefaultFontSizeFunction>();
  RegisterFunction<extensions::ClearDefaultFixedFontSizeFunction>();
  RegisterFunction<extensions::GetDefaultFixedFontSizeFunction>();
  RegisterFunction<extensions::SetDefaultFixedFontSizeFunction>();
  RegisterFunction<extensions::ClearMinimumFontSizeFunction>();
  RegisterFunction<extensions::GetMinimumFontSizeFunction>();
  RegisterFunction<extensions::SetMinimumFontSizeFunction>();

  // CloudPrint settings.
  RegisterFunction<extensions::CloudPrintSetupConnectorFunction>();
  RegisterFunction<extensions::CloudPrintGetHostNameFunction>();
  RegisterFunction<extensions::CloudPrintGetPrintersFunction>();

  // Experimental App API.
  RegisterFunction<extensions::AppNotifyFunction>();
  RegisterFunction<extensions::AppClearAllNotificationsFunction>();

  // Permissions
  RegisterFunction<ContainsPermissionsFunction>();
  RegisterFunction<GetAllPermissionsFunction>();
  RegisterFunction<RemovePermissionsFunction>();
  RegisterFunction<RequestPermissionsFunction>();

  // PageCapture
  RegisterFunction<extensions::PageCaptureSaveAsMHTMLFunction>();

  // TopSites
  RegisterFunction<extensions::GetTopSitesFunction>();

  // Serial
  RegisterFunction<extensions::SerialOpenFunction>();
  RegisterFunction<extensions::SerialCloseFunction>();
  RegisterFunction<extensions::SerialReadFunction>();
  RegisterFunction<extensions::SerialWriteFunction>();

  // Sockets
  RegisterFunction<extensions::SocketCreateFunction>();
  RegisterFunction<extensions::SocketDestroyFunction>();
  RegisterFunction<extensions::SocketConnectFunction>();
  RegisterFunction<extensions::SocketDisconnectFunction>();
  RegisterFunction<extensions::SocketReadFunction>();
  RegisterFunction<extensions::SocketWriteFunction>();

  // System
  RegisterFunction<extensions::GetIncognitoModeAvailabilityFunction>();
  RegisterFunction<extensions::GetUpdateStatusFunction>();

  // Net
  RegisterFunction<extensions::AddRulesFunction>();
  RegisterFunction<extensions::RemoveRulesFunction>();
  RegisterFunction<extensions::GetRulesFunction>();

  // Runtime
  RegisterFunction<extensions::RuntimeGetBackgroundPageFunction>();
  RegisterFunction<extensions::RuntimeReloadFunction>();
  RegisterFunction<extensions::RuntimeRequestUpdateCheckFunction>();

  // Generated APIs
  extensions::api::GeneratedFunctionRegistry::RegisterAll(this);
#endif  // defined(ENABLE_EXTENSIONS)
}

void ExtensionFunctionRegistry::GetAllNames(std::vector<std::string>* names) {
  for (FactoryMap::iterator iter = factories_.begin();
       iter != factories_.end(); ++iter) {
    names->push_back(iter->first);
  }
}

bool ExtensionFunctionRegistry::OverrideFunction(
    const std::string& name,
    ExtensionFunctionFactory factory) {
  FactoryMap::iterator iter = factories_.find(name);
  if (iter == factories_.end()) {
    return false;
  } else {
    iter->second = factory;
    return true;
  }
}

ExtensionFunction* ExtensionFunctionRegistry::NewFunction(
    const std::string& name) {
  FactoryMap::iterator iter = factories_.find(name);
  DCHECK(iter != factories_.end());
  ExtensionFunction* function = iter->second();
  function->set_name(name);
  return function;
}
