// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_registry.h"

#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/bookmarks/bookmark_extension_api.h"
#include "chrome/browser/bookmarks/bookmark_manager_extension_api.h"
#include "chrome/browser/download/download_extension_api.h"
#include "chrome/browser/extensions/api/app/app_api.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/api/declarative/declarative_api.h"
#include "chrome/browser/extensions/api/extension_action/extension_browser_actions_api.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/offscreen_tabs/offscreen_tabs_api.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/api/serial/serial_api.h"
#include "chrome/browser/extensions/api/socket/socket_api.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/execute_code_in_tab_function.h"
#include "chrome/browser/extensions/extension_chrome_auth_private_api.h"
#include "chrome/browser/extensions/extension_content_settings_api.h"
#include "chrome/browser/extensions/extension_context_menu_api.h"
#include "chrome/browser/extensions/extension_cookies_api.h"
#include "chrome/browser/extensions/extension_debugger_api.h"
#include "chrome/browser/extensions/extension_font_settings_api.h"
#include "chrome/browser/extensions/extension_i18n_api.h"
#include "chrome/browser/extensions/extension_idle_api.h"
#include "chrome/browser/extensions/extension_managed_mode_api.h"
#include "chrome/browser/extensions/extension_management_api.h"
#include "chrome/browser/extensions/extension_metrics_module.h"
#include "chrome/browser/extensions/extension_module.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_page_capture_api.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_record_api.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/browser/extensions/extension_web_socket_proxy_private_api.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/browser/extensions/settings/settings_api.h"
#include "chrome/browser/extensions/system/system_api.h"
#include "chrome/browser/history/history_extension_api.h"
#include "chrome/browser/history/top_sites_extension_api.h"
#include "chrome/browser/infobars/infobar_extension_api.h"
#include "chrome/browser/rlz/rlz_extension_api.h"
#include "chrome/browser/speech/speech_input_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/common/extensions/api/generated_api.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/extension_input_api.h"
#endif

#if defined(OS_CHROMEOS) && defined(USE_VIRTUAL_KEYBOARD)
#include "chrome/browser/extensions/extension_input_ui_api.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/file_browser_private_api.h"
#include "chrome/browser/chromeos/extensions/echo_private_api.h"
#include "chrome/browser/chromeos/media/media_player_extension_api.h"
#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"
#include "chrome/browser/extensions/extension_info_private_api_chromeos.h"
#include "chrome/browser/extensions/extension_input_ime_api.h"
#include "chrome/browser/extensions/extension_input_method_api.h"
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
  RegisterFunction<GetTabFunction>();
  RegisterFunction<GetCurrentTabFunction>();
  RegisterFunction<GetSelectedTabFunction>();
  RegisterFunction<GetAllTabsInWindowFunction>();
  RegisterFunction<QueryTabsFunction>();
  RegisterFunction<HighlightTabsFunction>();
  RegisterFunction<CreateTabFunction>();
  RegisterFunction<UpdateTabFunction>();
  RegisterFunction<MoveTabsFunction>();
  RegisterFunction<ReloadTabFunction>();
  RegisterFunction<RemoveTabsFunction>();
  RegisterFunction<DetectTabLanguageFunction>();
  RegisterFunction<CaptureVisibleTabFunction>();
  RegisterFunction<TabsExecuteScriptFunction>();
  RegisterFunction<TabsInsertCSSFunction>();

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
  RegisterFunction<RemoveServerBoundCertsFunction>();
  RegisterFunction<RemovePluginDataFunction>();
  RegisterFunction<RemovePasswordsFunction>();
  RegisterFunction<RemoveWebSQLFunction>();

  // Bookmarks.
  RegisterFunction<GetBookmarksFunction>();
  RegisterFunction<GetBookmarkChildrenFunction>();
  RegisterFunction<GetBookmarkRecentFunction>();
  RegisterFunction<GetBookmarkTreeFunction>();
  RegisterFunction<GetBookmarkSubTreeFunction>();
  RegisterFunction<SearchBookmarksFunction>();
  RegisterFunction<RemoveBookmarkFunction>();
  RegisterFunction<RemoveTreeBookmarkFunction>();
  RegisterFunction<CreateBookmarkFunction>();
  RegisterFunction<MoveBookmarkFunction>();
  RegisterFunction<UpdateBookmarkFunction>();

  // Infobars.
  RegisterFunction<ShowInfoBarFunction>();

  // BookmarkManager
  RegisterFunction<CopyBookmarkManagerFunction>();
  RegisterFunction<CutBookmarkManagerFunction>();
  RegisterFunction<PasteBookmarkManagerFunction>();
  RegisterFunction<CanPasteBookmarkManagerFunction>();
  RegisterFunction<ImportBookmarksFunction>();
  RegisterFunction<ExportBookmarksFunction>();
  RegisterFunction<SortChildrenBookmarkManagerFunction>();
  RegisterFunction<BookmarkManagerGetStringsFunction>();
  RegisterFunction<StartDragBookmarkManagerFunction>();
  RegisterFunction<DropBookmarkManagerFunction>();
  RegisterFunction<GetSubtreeBookmarkManagerFunction>();
  RegisterFunction<CanEditBookmarkManagerFunction>();

  // History
  RegisterFunction<AddUrlHistoryFunction>();
  RegisterFunction<DeleteAllHistoryFunction>();
  RegisterFunction<DeleteRangeHistoryFunction>();
  RegisterFunction<DeleteUrlHistoryFunction>();
  RegisterFunction<GetVisitsHistoryFunction>();
  RegisterFunction<SearchHistoryFunction>();

  // Idle
  RegisterFunction<ExtensionIdleQueryStateFunction>();

  // I18N.
  RegisterFunction<GetAcceptLanguagesFunction>();

  // Processes.
  RegisterFunction<GetProcessIdForTabFunction>();

  // Metrics.
  RegisterFunction<MetricsRecordUserActionFunction>();
  RegisterFunction<MetricsRecordValueFunction>();
  RegisterFunction<MetricsRecordPercentageFunction>();
  RegisterFunction<MetricsRecordCountFunction>();
  RegisterFunction<MetricsRecordSmallCountFunction>();
  RegisterFunction<MetricsRecordMediumCountFunction>();
  RegisterFunction<MetricsRecordTimeFunction>();
  RegisterFunction<MetricsRecordMediumTimeFunction>();
  RegisterFunction<MetricsRecordLongTimeFunction>();

  // Record.
  RegisterFunction<CaptureURLsFunction>();
  RegisterFunction<ReplayURLsFunction>();

  // RLZ.
#if defined(OS_WIN) || defined(OS_MACOSX)
  RegisterFunction<RlzRecordProductEventFunction>();
  RegisterFunction<RlzGetAccessPointRlzFunction>();
  RegisterFunction<RlzSendFinancialPingFunction>();
  RegisterFunction<RlzClearProductStateFunction>();
#endif

  // Cookies.
  RegisterFunction<GetCookieFunction>();
  RegisterFunction<GetAllCookiesFunction>();
  RegisterFunction<SetCookieFunction>();
  RegisterFunction<RemoveCookieFunction>();
  RegisterFunction<GetAllCookieStoresFunction>();

  // Test.
  RegisterFunction<ExtensionTestPassFunction>();
  RegisterFunction<ExtensionTestFailFunction>();
  RegisterFunction<ExtensionTestLogFunction>();
  RegisterFunction<ExtensionTestQuotaResetFunction>();
  RegisterFunction<ExtensionTestCreateIncognitoTabFunction>();
  RegisterFunction<ExtensionTestSendMessageFunction>();
  RegisterFunction<ExtensionTestGetConfigFunction>();

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

  // Context Menus.
  RegisterFunction<CreateContextMenuFunction>();
  RegisterFunction<UpdateContextMenuFunction>();
  RegisterFunction<RemoveContextMenuFunction>();
  RegisterFunction<RemoveAllContextMenusFunction>();

  // Omnibox.
  RegisterFunction<extensions::OmniboxSendSuggestionsFunction>();
  RegisterFunction<extensions::OmniboxSetDefaultSuggestionFunction>();

  // Speech input.
  RegisterFunction<StartSpeechInputFunction>();
  RegisterFunction<StopSpeechInputFunction>();
  RegisterFunction<IsRecordingSpeechInputFunction>();

#if defined(TOOLKIT_VIEWS)
  // Input.
  RegisterFunction<SendKeyboardEventInputFunction>();
#endif

#if defined(USE_VIRTUAL_KEYBOARD)
  RegisterFunction<HideKeyboardFunction>();
  RegisterFunction<SetKeyboardHeightFunction>();
#endif

#if defined(OS_CHROMEOS)
  // IME
  RegisterFunction<SetCompositionFunction>();
  RegisterFunction<ClearCompositionFunction>();
  RegisterFunction<CommitTextFunction>();
  RegisterFunction<SetCandidateWindowPropertiesFunction>();
  RegisterFunction<SetCandidatesFunction>();
  RegisterFunction<SetCursorPositionFunction>();
  RegisterFunction<SetMenuItemsFunction>();
  RegisterFunction<UpdateMenuItemsFunction>();

  RegisterFunction<InputEventHandled>();
#if defined(USE_VIRTUAL_KEYBOARD)
  RegisterFunction<CandidateClickedInputUiFunction>();
  RegisterFunction<CursorUpInputUiFunction>();
  RegisterFunction<CursorDownInputUiFunction>();
  RegisterFunction<PageUpInputUiFunction>();
  RegisterFunction<PageDownInputUiFunction>();
  RegisterFunction<RegisterInputUiFunction>();
  RegisterFunction<PageUpInputUiFunction>();
  RegisterFunction<PageDownInputUiFunction>();
#endif
#endif

  // Managed mode.
  RegisterFunction<GetManagedModeFunction>();
  RegisterFunction<EnterManagedModeFunction>();

  // Management.
  RegisterFunction<GetAllExtensionsFunction>();
  RegisterFunction<GetExtensionByIdFunction>();
  RegisterFunction<GetPermissionWarningsByIdFunction>();
  RegisterFunction<GetPermissionWarningsByManifestFunction>();
  RegisterFunction<LaunchAppFunction>();
  RegisterFunction<SetEnabledFunction>();
  RegisterFunction<UninstallFunction>();

  // Extension module.
  RegisterFunction<SetUpdateUrlDataFunction>();
  RegisterFunction<IsAllowedIncognitoAccessFunction>();
  RegisterFunction<IsAllowedFileSchemeAccessFunction>();

  // WebstorePrivate.
  RegisterFunction<GetBrowserLoginFunction>();
  RegisterFunction<GetStoreLoginFunction>();
  RegisterFunction<SetStoreLoginFunction>();
  RegisterFunction<InstallBundleFunction>();
  RegisterFunction<BeginInstallWithManifestFunction>();
  RegisterFunction<CompleteInstallFunction>();
  RegisterFunction<SilentlyInstallFunction>();
  RegisterFunction<GetWebGLStatusFunction>();

  // WebNavigation.
  RegisterFunction<extensions::GetFrameFunction>();
  RegisterFunction<extensions::GetAllFramesFunction>();

  // WebRequest.
  RegisterFunction<WebRequestAddEventListener>();
  RegisterFunction<WebRequestEventHandled>();
  RegisterFunction<WebRequestHandlerBehaviorChanged>();

  // Preferences.
  RegisterFunction<GetPreferenceFunction>();
  RegisterFunction<SetPreferenceFunction>();
  RegisterFunction<ClearPreferenceFunction>();

  // ChromeOS-specific part of the API.
#if defined(OS_CHROMEOS)
  // Device Customization.
  RegisterFunction<GetChromeosInfoFunction>();

  // FileBrowserPrivate functions.
  // TODO(jamescook): Expose these on non-ChromeOS platforms so we can use
  // the extension-based file picker on Aura. crbug.com/97424
  RegisterFunction<CancelFileDialogFunction>();
  RegisterFunction<ExecuteTasksFileBrowserFunction>();
  RegisterFunction<FileDialogStringsFunction>();
  RegisterFunction<GetFileTasksFileBrowserFunction>();
  RegisterFunction<GetVolumeMetadataFunction>();
  RegisterFunction<RequestLocalFileSystemFunction>();
  RegisterFunction<AddFileWatchBrowserFunction>();
  RegisterFunction<RemoveFileWatchBrowserFunction>();
  RegisterFunction<SelectFileFunction>();
  RegisterFunction<SelectFilesFunction>();
  RegisterFunction<AddMountFunction>();
  RegisterFunction<RemoveMountFunction>();
  RegisterFunction<GetMountPointsFunction>();
  RegisterFunction<GetSizeStatsFunction>();
  RegisterFunction<FormatDeviceFunction>();
  RegisterFunction<ViewFilesFunction>();
  RegisterFunction<ToggleFullscreenFunction>();
  RegisterFunction<IsFullscreenFunction>();
  RegisterFunction<GetGDataFilePropertiesFunction>();
  RegisterFunction<PinGDataFileFunction>();
  RegisterFunction<GetFileLocationsFunction>();
  RegisterFunction<GetGDataFilesFunction>();
  RegisterFunction<GetFileTransfersFunction>();
  RegisterFunction<CancelFileTransfersFunction>();
  RegisterFunction<TransferFileFunction>();
  RegisterFunction<GetGDataPreferencesFunction>();
  RegisterFunction<SetGDataPreferencesFunction>();

  // Mediaplayer
  RegisterFunction<PlayMediaplayerFunction>();
  RegisterFunction<GetPlaylistMediaplayerFunction>();
  RegisterFunction<SetWindowHeightMediaplayerFunction>();
  RegisterFunction<CloseWindowMediaplayerFunction>();

  // InputMethod
  RegisterFunction<GetInputMethodFunction>();

  // Echo
  RegisterFunction<GetRegistrationCodeFunction>();

  // Terminal
  RegisterFunction<OpenTerminalProcessFunction>();
  RegisterFunction<SendInputToTerminalProcessFunction>();
  RegisterFunction<CloseTerminalProcessFunction>();
  RegisterFunction<OnTerminalResizeFunction>();

#if defined(USE_VIRTUAL_KEYBOARD)
  // Input
  RegisterFunction<SendHandwritingStrokeFunction>();
  RegisterFunction<CancelHandwritingStrokesFunction>();
#endif
#endif

  // Websocket to TCP proxy. Currently noop on anything other than ChromeOS.
  RegisterFunction<WebSocketProxyPrivateGetPassportForTCPFunction>();
  RegisterFunction<WebSocketProxyPrivateGetURLForTCPFunction>();

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
  RegisterFunction<GetResourceIdentifiersFunction>();
  RegisterFunction<ClearContentSettingsFunction>();
  RegisterFunction<GetContentSettingFunction>();
  RegisterFunction<SetContentSettingFunction>();

  // Font settings.
  RegisterFunction<GetFontListFunction>();
  RegisterFunction<ClearFontFunction>();
  RegisterFunction<GetFontFunction>();
  RegisterFunction<SetFontFunction>();
  RegisterFunction<ClearDefaultFontSizeFunction>();
  RegisterFunction<GetDefaultFontSizeFunction>();
  RegisterFunction<SetDefaultFontSizeFunction>();
  RegisterFunction<ClearDefaultFixedFontSizeFunction>();
  RegisterFunction<GetDefaultFixedFontSizeFunction>();
  RegisterFunction<SetDefaultFixedFontSizeFunction>();
  RegisterFunction<ClearMinimumFontSizeFunction>();
  RegisterFunction<GetMinimumFontSizeFunction>();
  RegisterFunction<SetMinimumFontSizeFunction>();
  RegisterFunction<ClearDefaultCharacterSetFunction>();
  RegisterFunction<GetDefaultCharacterSetFunction>();
  RegisterFunction<SetDefaultCharacterSetFunction>();

  // ChromeAuth settings.
  RegisterFunction<SetCloudPrintCredentialsFunction>();

  // Experimental App API.
  RegisterFunction<extensions::AppNotifyFunction>();
  RegisterFunction<extensions::AppClearAllNotificationsFunction>();

  // Permissions
  RegisterFunction<ContainsPermissionsFunction>();
  RegisterFunction<GetAllPermissionsFunction>();
  RegisterFunction<RemovePermissionsFunction>();
  RegisterFunction<RequestPermissionsFunction>();

  // Downloads
  RegisterFunction<DownloadsDownloadFunction>();
  RegisterFunction<DownloadsSearchFunction>();
  RegisterFunction<DownloadsPauseFunction>();
  RegisterFunction<DownloadsResumeFunction>();
  RegisterFunction<DownloadsCancelFunction>();
  RegisterFunction<DownloadsEraseFunction>();
  RegisterFunction<DownloadsSetDestinationFunction>();
  RegisterFunction<DownloadsAcceptDangerFunction>();
  RegisterFunction<DownloadsShowFunction>();
  RegisterFunction<DownloadsDragFunction>();
  RegisterFunction<DownloadsGetFileIconFunction>();

  // PageCapture
  RegisterFunction<PageCaptureSaveAsMHTMLFunction>();

  // TopSites
  RegisterFunction<GetTopSitesFunction>();

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

  // Experimental Offscreen Tabs
  RegisterFunction<CreateOffscreenTabFunction>();
  RegisterFunction<GetOffscreenTabFunction>();
  RegisterFunction<GetAllOffscreenTabFunction>();
  RegisterFunction<RemoveOffscreenTabFunction>();
  RegisterFunction<SendKeyboardEventOffscreenTabFunction>();
  RegisterFunction<SendMouseEventOffscreenTabFunction>();
  RegisterFunction<ToDataUrlOffscreenTabFunction>();
  RegisterFunction<UpdateOffscreenTabFunction>();

  // Identity
  RegisterFunction<extensions::GetAuthTokenFunction>();

  // Runtime
  RegisterFunction<extensions::RuntimeGetBackgroundPageFunction>();

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
