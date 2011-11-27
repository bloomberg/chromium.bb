// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_dispatcher.h"

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/process_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_extension_api.h"
#include "chrome/browser/bookmarks/bookmark_manager_extension_api.h"
#include "chrome/browser/download/download_extension_api.h"
#include "chrome/browser/extensions/execute_code_in_tab_function.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_app_api.h"
#include "chrome/browser/extensions/extension_browser_actions_api.h"
#include "chrome/browser/extensions/extension_chrome_auth_private_api.h"
#include "chrome/browser/extensions/extension_clear_api.h"
#include "chrome/browser/extensions/extension_content_settings_api.h"
#include "chrome/browser/extensions/extension_context_menu_api.h"
#include "chrome/browser/extensions/extension_cookies_api.h"
#include "chrome/browser/extensions/extension_debugger_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_i18n_api.h"
#include "chrome/browser/extensions/extension_idle_api.h"
#include "chrome/browser/extensions/extension_management_api.h"
#include "chrome/browser/extensions/extension_metrics_module.h"
#include "chrome/browser/extensions/extension_module.h"
#include "chrome/browser/extensions/extension_omnibox_api.h"
#include "chrome/browser/extensions/extension_page_actions_module.h"
#include "chrome/browser/extensions/extension_permissions_api.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_proxy_api.h"
#include "chrome/browser/extensions/extension_save_page_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_api.h"
#include "chrome/browser/extensions/extension_sidebar_api.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/browser/extensions/extension_tts_api.h"
#include "chrome/browser/extensions/extension_tts_engine_api.h"
#include "chrome/browser/extensions/extension_web_socket_proxy_private_api.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/history/history_extension_api.h"
#include "chrome/browser/history/top_sites_extension_api.h"
#include "chrome/browser/infobars/infobar_extension_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/rlz/rlz_extension_api.h"
#include "chrome/browser/speech/speech_input_extension_api.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/extension_input_api.h"
#endif

#if defined(OS_CHROMEOS) && defined(TOUCH_UI)
#include "chrome/browser/extensions/extension_input_ui_api.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/file_browser_extension_api.h"
#include "chrome/browser/chromeos/extensions/get_chromeos_info_extension_api.h"
#include "chrome/browser/chromeos/extensions/media_player_extension_api.h"
#include "chrome/browser/chromeos/extensions/input_ime_extension_api.h"
#include "chrome/browser/chromeos/extensions/input_method_extension_api.h"
#endif

using extensions::ExtensionAPI;

// FactoryRegistry -------------------------------------------------------------

namespace {

// Template for defining ExtensionFunctionFactory.
template<class T>
ExtensionFunction* NewExtensionFunction() {
  return new T();
}

// Contains a list of all known extension functions and allows clients to
// create instances of them.
class FactoryRegistry {
 public:
  static FactoryRegistry* GetInstance();
  FactoryRegistry() { ResetFunctions(); }

  // Resets all functions to their default values.
  void ResetFunctions();

  // Adds all function names to 'names'.
  void GetAllNames(std::vector<std::string>* names);

  // Allows overriding of specific functions (e.g. for testing).  Functions
  // must be previously registered.  Returns true if successful.
  bool OverrideFunction(const std::string& name,
                        ExtensionFunctionFactory factory);

  // Factory method for the ExtensionFunction registered as 'name'.
  ExtensionFunction* NewFunction(const std::string& name);

 private:
  template<class T>
  void RegisterFunction() {
    factories_[T::function_name()] = &NewExtensionFunction<T>;
  }

  typedef std::map<std::string, ExtensionFunctionFactory> FactoryMap;
  FactoryMap factories_;
};

FactoryRegistry* FactoryRegistry::GetInstance() {
  return Singleton<FactoryRegistry>::get();
}

void FactoryRegistry::ResetFunctions() {
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
  RegisterFunction<EnablePageActionFunction>();
  RegisterFunction<DisablePageActionFunction>();
  RegisterFunction<PageActionShowFunction>();
  RegisterFunction<PageActionHideFunction>();
  RegisterFunction<PageActionSetIconFunction>();
  RegisterFunction<PageActionSetTitleFunction>();
  RegisterFunction<PageActionSetPopupFunction>();

  // Browser Actions.
  RegisterFunction<BrowserActionSetIconFunction>();
  RegisterFunction<BrowserActionSetTitleFunction>();
  RegisterFunction<BrowserActionSetBadgeTextFunction>();
  RegisterFunction<BrowserActionSetBadgeBackgroundColorFunction>();
  RegisterFunction<BrowserActionSetPopupFunction>();

  // Browsing Data.
  RegisterFunction<ClearBrowsingDataFunction>();
  RegisterFunction<ClearCacheFunction>();
  RegisterFunction<ClearCookiesFunction>();
  RegisterFunction<ClearDownloadsFunction>();
  RegisterFunction<ClearFormDataFunction>();
  RegisterFunction<ClearHistoryFunction>();
  RegisterFunction<ClearPasswordsFunction>();

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

  // RLZ.
#if defined(OS_WIN)
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
  RegisterFunction<OmniboxSendSuggestionsFunction>();
  RegisterFunction<OmniboxSetDefaultSuggestionFunction>();

  // Sidebar.
  RegisterFunction<CollapseSidebarFunction>();
  RegisterFunction<ExpandSidebarFunction>();
  RegisterFunction<GetStateSidebarFunction>();
  RegisterFunction<HideSidebarFunction>();
  RegisterFunction<NavigateSidebarFunction>();
  RegisterFunction<SetBadgeTextSidebarFunction>();
  RegisterFunction<SetIconSidebarFunction>();
  RegisterFunction<SetTitleSidebarFunction>();
  RegisterFunction<ShowSidebarFunction>();

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
#if defined(TOUCH_UI)
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
  RegisterFunction<BeginInstallWithManifestFunction>();
  RegisterFunction<CompleteInstallFunction>();
  RegisterFunction<SilentlyInstallFunction>();

  // WebNavigation.
  RegisterFunction<GetFrameFunction>();
  RegisterFunction<GetAllFramesFunction>();

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

  // Mediaplayer
  RegisterFunction<PlayAtMediaplayerFunction>();
  RegisterFunction<SetPlaybackErrorMediaplayerFunction>();
  RegisterFunction<GetPlaylistMediaplayerFunction>();
  RegisterFunction<TogglePlaylistPanelMediaplayerFunction>();
  RegisterFunction<ToggleFullscreenMediaplayerFunction>();

  // InputMethod
  RegisterFunction<GetInputMethodFunction>();

#if defined(TOUCH_UI)
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

  // Content settings.
  RegisterFunction<GetResourceIdentifiersFunction>();
  RegisterFunction<ClearContentSettingsFunction>();
  RegisterFunction<GetContentSettingFunction>();
  RegisterFunction<SetContentSettingFunction>();

  // ChromeAuth settings.
  RegisterFunction<SetCloudPrintCredentialsFunction>();

  // Experimental App API.
  RegisterFunction<AppNotifyFunction>();
  RegisterFunction<AppClearAllNotificationsFunction>();

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

  // SavePage
  RegisterFunction<SavePageAsMHTMLFunction>();

  // TopSites
  RegisterFunction<GetTopSitesFunction>();
}

void FactoryRegistry::GetAllNames(std::vector<std::string>* names) {
  for (FactoryMap::iterator iter = factories_.begin();
       iter != factories_.end(); ++iter) {
    names->push_back(iter->first);
  }
}

bool FactoryRegistry::OverrideFunction(const std::string& name,
                                       ExtensionFunctionFactory factory) {
  FactoryMap::iterator iter = factories_.find(name);
  if (iter == factories_.end()) {
    return false;
  } else {
    iter->second = factory;
    return true;
  }
}

ExtensionFunction* FactoryRegistry::NewFunction(const std::string& name) {
  FactoryMap::iterator iter = factories_.find(name);
  DCHECK(iter != factories_.end());
  ExtensionFunction* function = iter->second();
  function->set_name(name);
  return function;
}

};  // namespace

// ExtensionFunctionDispatcher -------------------------------------------------

void ExtensionFunctionDispatcher::GetAllFunctionNames(
    std::vector<std::string>* names) {
  FactoryRegistry::GetInstance()->GetAllNames(names);
}

bool ExtensionFunctionDispatcher::OverrideFunction(
    const std::string& name, ExtensionFunctionFactory factory) {
  return FactoryRegistry::GetInstance()->OverrideFunction(name, factory);
}

void ExtensionFunctionDispatcher::ResetFunctions() {
  FactoryRegistry::GetInstance()->ResetFunctions();
}

// static
void ExtensionFunctionDispatcher::DispatchOnIOThread(
    ExtensionInfoMap* extension_info_map,
    void* profile,
    int render_process_id,
    base::WeakPtr<ChromeRenderMessageFilter> ipc_sender,
    int routing_id,
    const ExtensionHostMsg_Request_Params& params) {
  const Extension* extension =
      extension_info_map->extensions().GetByID(params.extension_id);

  scoped_refptr<ExtensionFunction> function(
      CreateExtensionFunction(params, extension, render_process_id,
                              extension_info_map->process_map(), profile,
                              ipc_sender, routing_id));
  if (!function)
    return;

  IOThreadExtensionFunction* function_io =
      function->AsIOThreadExtensionFunction();
  if (!function_io) {
    NOTREACHED();
    return;
  }
  function_io->set_ipc_sender(ipc_sender, routing_id);
  function_io->set_extension_info_map(extension_info_map);
  function->set_include_incognito(
      extension_info_map->IsIncognitoEnabled(extension->id()));

  ExtensionsQuotaService* quota = extension_info_map->quota_service();
  if (quota->Assess(extension->id(), function, &params.arguments,
                    base::TimeTicks::Now())) {
    function->Run();
  } else {
    function->OnQuotaExceeded();
  }
}

ExtensionFunctionDispatcher::ExtensionFunctionDispatcher(Profile* profile,
                                                         Delegate* delegate)
  : profile_(profile),
    delegate_(delegate) {
}

ExtensionFunctionDispatcher::~ExtensionFunctionDispatcher() {
}

Browser* ExtensionFunctionDispatcher::GetCurrentBrowser(
    RenderViewHost* render_view_host, bool include_incognito) {
  Browser* browser = delegate_->GetBrowser();

  // If the delegate has an associated browser, that is always the right answer.
  if (browser)
    return browser;

  // Otherwise, try to default to a reasonable browser. If |include_incognito|
  // is true, we will also search browsers in the incognito version of this
  // profile. Note that the profile may already be incognito, in which case
  // we will search the incognito version only, regardless of the value of
  // |include_incognito|.
  Profile* profile = Profile::FromBrowserContext(
      render_view_host->process()->GetBrowserContext());
  browser = BrowserList::FindTabbedBrowser(profile, include_incognito);

  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a background_page onload chrome.tabs api call can make it into here
  // before the browser is sufficiently initialized to return here.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return browser;
}

void ExtensionFunctionDispatcher::Dispatch(
    const ExtensionHostMsg_Request_Params& params,
    RenderViewHost* render_view_host) {
  ExtensionService* service = profile()->GetExtensionService();
  extensions::ProcessMap* process_map = service->process_map();
  if (!service || !process_map)
    return;

  const Extension* extension = service->GetExtensionById(
      params.extension_id, false);
  if (!extension)
    extension = service->GetExtensionByWebExtent(params.source_url);

  scoped_refptr<ExtensionFunction> function(
      CreateExtensionFunction(params, extension,
                              render_view_host->process()->GetID(),
                              *(service->process_map()),
                              profile(), render_view_host,
                              render_view_host->routing_id()));
  if (!function)
    return;

  UIThreadExtensionFunction* function_ui =
      function->AsUIThreadExtensionFunction();
  if (!function_ui) {
    NOTREACHED();
    return;
  }
  function_ui->SetRenderViewHost(render_view_host);
  function_ui->set_dispatcher(AsWeakPtr());
  function_ui->set_profile(profile_);
  function->set_include_incognito(service->CanCrossIncognito(extension));

  ExtensionsQuotaService* quota = service->quota_service();
  if (quota->Assess(extension->id(), function, &params.arguments,
                    base::TimeTicks::Now())) {
    // See crbug.com/39178.
    ExternalProtocolHandler::PermitLaunchUrl();

    function->Run();
  } else {
    function->OnQuotaExceeded();
  }
}

// static
ExtensionFunction* ExtensionFunctionDispatcher::CreateExtensionFunction(
    const ExtensionHostMsg_Request_Params& params,
    const Extension* extension,
    int requesting_process_id,
    const extensions::ProcessMap& process_map,
    void* profile,
    IPC::Message::Sender* ipc_sender,
    int routing_id) {
  if (!extension) {
    LOG(ERROR) << "Specified extension does not exist.";
    SendAccessDenied(ipc_sender, routing_id, params.request_id);
    return NULL;
  }

  if (ExtensionAPI::GetInstance()->IsPrivileged(params.name) &&
      !process_map.Contains(extension->id(), requesting_process_id)) {
    LOG(ERROR) << "Extension API called from incorrect process "
               << requesting_process_id
               << " from URL " << params.source_url.spec();
    SendAccessDenied(ipc_sender, routing_id, params.request_id);
    return NULL;
  }

  if (!extension->HasAPIPermission(params.name)) {
    LOG(ERROR) << "Extension " << extension->id() << " does not have "
               << "permission to function: " << params.name;
    SendAccessDenied(ipc_sender, routing_id, params.request_id);
    return NULL;
  }

  ExtensionFunction* function =
      FactoryRegistry::GetInstance()->NewFunction(params.name);
  function->SetArgs(&params.arguments);
  function->set_source_url(params.source_url);
  function->set_request_id(params.request_id);
  function->set_has_callback(params.has_callback);
  function->set_user_gesture(params.user_gesture);
  function->set_extension(extension);
  function->set_profile_id(profile);
  return function;
}

// static
void ExtensionFunctionDispatcher::SendAccessDenied(
    IPC::Message::Sender* ipc_sender, int routing_id, int request_id) {
  ipc_sender->Send(new ExtensionMsg_Response(
      routing_id, request_id, false, std::string(),
      "Access to extension API denied."));
}
