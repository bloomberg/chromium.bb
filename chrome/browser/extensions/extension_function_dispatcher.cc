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
#include "chrome/browser/extensions/execute_code_in_tab_function.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_bookmark_manager_api.h"
#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/browser/extensions/extension_browser_actions_api.h"
#include "chrome/browser/extensions/extension_context_menu_api.h"
#include "chrome/browser/extensions/extension_cookies_api.h"
#include "chrome/browser/extensions/extension_debugger_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_history_api.h"
#include "chrome/browser/extensions/extension_i18n_api.h"
#include "chrome/browser/extensions/extension_idle_api.h"
#include "chrome/browser/extensions/extension_infobar_module.h"
#include "chrome/browser/extensions/extension_management_api.h"
#include "chrome/browser/extensions/extension_metrics_module.h"
#include "chrome/browser/extensions/extension_module.h"
#include "chrome/browser/extensions/extension_omnibox_api.h"
#include "chrome/browser/extensions/extension_page_actions_module.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_proxy_api.h"
#include "chrome/browser/extensions/extension_rlz_module.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sidebar_api.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/browser/extensions/extension_tts_api.h"
#include "chrome/browser/extensions/extension_web_socket_proxy_private_api.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
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
#include "chrome/browser/extensions/extension_file_browser_private_api.h"
#include "chrome/browser/extensions/extension_info_private_api_chromeos.h"
#endif

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
  RegisterFunction<CreateTabFunction>();
  RegisterFunction<UpdateTabFunction>();
  RegisterFunction<MoveTabFunction>();
  RegisterFunction<RemoveTabFunction>();
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

  // Bookmarks.
  RegisterFunction<GetBookmarksFunction>();
  RegisterFunction<GetBookmarkChildrenFunction>();
  RegisterFunction<GetBookmarkRecentFunction>();
  RegisterFunction<GetBookmarkTreeFunction>();
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
  RegisterFunction<MetricsGetEnabledFunction>();
  RegisterFunction<MetricsSetEnabledFunction>();
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
  RegisterFunction<ExtensionTtsSpeakFunction>();
  RegisterFunction<ExtensionTtsStopSpeakingFunction>();
  RegisterFunction<ExtensionTtsIsSpeakingFunction>();
  RegisterFunction<ExtensionTtsSpeakCompletedFunction>();

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

#if defined(TOOLKIT_VIEWS)
  // Input.
  RegisterFunction<SendKeyboardEventInputFunction>();
#endif

#if defined(TOUCH_UI)
  RegisterFunction<HideKeyboardFunction>();
#endif

#if defined(OS_CHROMEOS) && defined(TOUCH_UI)
  // IME
  RegisterFunction<CandidateClickedInputUiFunction>();
  RegisterFunction<CursorUpInputUiFunction>();
  RegisterFunction<CursorDownInputUiFunction>();
  RegisterFunction<PageUpInputUiFunction>();
  RegisterFunction<PageDownInputUiFunction>();
  RegisterFunction<RegisterInputUiFunction>();
  RegisterFunction<PageUpInputUiFunction>();
  RegisterFunction<PageDownInputUiFunction>();
#endif

  // Management.
  RegisterFunction<GetAllExtensionsFunction>();
  RegisterFunction<GetExtensionByIdFunction>();
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
  RegisterFunction<PromptBrowserLoginFunction>();
  RegisterFunction<BeginInstallFunction>();
  RegisterFunction<BeginInstallWithManifestFunction>();
  RegisterFunction<CompleteInstallFunction>();

  // WebRequest.
  RegisterFunction<WebRequestAddEventListener>();
  RegisterFunction<WebRequestEventHandled>();

  // Preferences.
  RegisterFunction<GetPreferenceFunction>();
  RegisterFunction<SetPreferenceFunction>();
  RegisterFunction<ClearPreferenceFunction>();

  // ChromeOS-specific part of the API.
#if defined(OS_CHROMEOS)
  // Device Customization.
  RegisterFunction<GetChromeosInfoFunction>();

  // FileBrowserPrivate functions.
  RegisterFunction<CancelFileDialogFunction>();
  RegisterFunction<ExecuteTasksFileBrowserFunction>();
  RegisterFunction<FileDialogStringsFunction>();
  RegisterFunction<GetFileTasksFileBrowserFunction>();
  RegisterFunction<RequestLocalFileSystemFunction>();
  RegisterFunction<SelectFileFunction>();
  RegisterFunction<SelectFilesFunction>();
  RegisterFunction<ViewFilesFunction>();

#if defined(TOUCH_UI)
  // Input
  RegisterFunction<SendHandwritingStrokeFunction>();
  RegisterFunction<CancelHandwritingStrokesFunction>();
#endif
#endif

  // Websocket to TCP proxy. Currently noop on anything other than ChromeOS.
  RegisterFunction<WebSocketProxyPrivateGetPassportForTCPFunction>();

  // Debugger
  RegisterFunction<AttachDebuggerFunction>();
  RegisterFunction<DetachDebuggerFunction>();
  RegisterFunction<SendRequestDebuggerFunction>();
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

ExtensionFunctionDispatcher::ExtensionFunctionDispatcher(Profile* profile,
                                                         Delegate* delegate)
  : profile_(profile),
    delegate_(delegate),
    ALLOW_THIS_IN_INITIALIZER_LIST(peer_(new Peer(this))) {
}

ExtensionFunctionDispatcher::~ExtensionFunctionDispatcher() {
  peer_->dispatcher_ = NULL;
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
  Profile* profile = render_view_host->process()->profile();
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
  // TODO(aa): It would be cool to use ExtensionProcessManager to track which
  // processes are extension processes rather than ChildProcessSecurityPolicy.
  // EPM has richer information: it not only knows which processes contain
  // at least one extension, but it knows which extensions are inside and what
  // permissions the have. So we would be able to enforce permissions more
  // granularly.
  if (!ChildProcessSecurityPolicy::GetInstance()->HasExtensionBindings(
          render_view_host->process()->id())) {
    // TODO(aa): Allow content scripts access to low-threat extension APIs.
    // See: crbug.com/80308.
    LOG(ERROR) << "Extension API called from non-extension process.";
    SendAccessDenied(render_view_host, params.request_id);
    return;
  }

  ExtensionService* service = profile()->GetExtensionService();
  if (!service)
    return;

  if (!service->ExtensionBindingsAllowed(params.source_url)) {
    LOG(ERROR) << "Extension bindings not allowed for URL: "
               << params.source_url.spec();
    SendAccessDenied(render_view_host, params.request_id);
    return;
  }

  // TODO(aa): When we allow content scripts to call extension APIs, we will
  // have to pass the extension ID explicitly here, not use the source URL.
  const Extension* extension = service->GetExtensionByURL(params.source_url);
  if (!extension)
    extension = service->GetExtensionByWebExtent(params.source_url);
  if (!extension) {
    LOG(ERROR) << "Extension does not exist for URL: "
               << params.source_url.spec();
    SendAccessDenied(render_view_host, params.request_id);
    return;
  }

  if (!extension->HasApiPermission(params.name)) {
    LOG(ERROR) << "Extension " << extension->id() << " does not have "
               << "permission to function: " << params.name;
    SendAccessDenied(render_view_host, params.request_id);
    return;
  }

  scoped_refptr<ExtensionFunction> function(
      FactoryRegistry::GetInstance()->NewFunction(params.name));
  function->SetRenderViewHost(render_view_host);
  function->set_dispatcher_peer(peer_);
  function->set_profile(profile_);
  function->set_extension_id(extension->id());
  function->SetArgs(&params.arguments);
  function->set_source_url(params.source_url);
  function->set_request_id(params.request_id);
  function->set_has_callback(params.has_callback);
  function->set_user_gesture(params.user_gesture);
  function->set_include_incognito(service->CanCrossIncognito(extension));

  ExtensionsQuotaService* quota = service->quota_service();
  if (quota->Assess(extension->id(), function, &params.arguments,
                    base::TimeTicks::Now())) {
    // See crbug.com/39178.
    ExternalProtocolHandler::PermitLaunchUrl();

    function->Run();
  } else {
    render_view_host->Send(new ExtensionMsg_Response(
        render_view_host->routing_id(), function->request_id(), false,
        std::string(), QuotaLimitHeuristic::kGenericOverQuotaError));
  }
}

void ExtensionFunctionDispatcher::SendAccessDenied(
    RenderViewHost* render_view_host, int request_id) {
  render_view_host->Send(new ExtensionMsg_Response(
      render_view_host->routing_id(), request_id, false, std::string(),
      "Access to extension API denied."));
}
