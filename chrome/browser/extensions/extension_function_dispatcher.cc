// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_dispatcher.h"

#include <map>

#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/execute_code_in_tab_function.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_bookmark_manager_api.h"
#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/browser/extensions/extension_browser_actions_api.h"
#include "chrome/browser/extensions/extension_clipboard_api.h"
#include "chrome/browser/extensions/extension_context_menu_api.h"
#include "chrome/browser/extensions/extension_cookies_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_history_api.h"
#include "chrome/browser/extensions/extension_i18n_api.h"
#include "chrome/browser/extensions/extension_idle_api.h"
#include "chrome/browser/extensions/extension_infobar_module.h"
#include "chrome/browser/extensions/extension_management_api.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_metrics_module.h"
#include "chrome/browser/extensions/extension_module.h"
#include "chrome/browser/extensions/extension_omnibox_api.h"
#include "chrome/browser/extensions/extension_page_actions_module.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_proxy_api.h"
#include "chrome/browser/extensions/extension_rlz_module.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sidebar_api.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/browser/extensions/extension_tts_api.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"
#include "chrome/browser/webui/favicon_source.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/url_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/extension_input_api.h"
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

  // Clipboard.
  RegisterFunction<ExecuteCopyClipboardFunction>();
  RegisterFunction<ExecuteCutClipboardFunction>();
  RegisterFunction<ExecutePasteClipboardFunction>();

  // Context Menus.
  RegisterFunction<CreateContextMenuFunction>();
  RegisterFunction<UpdateContextMenuFunction>();
  RegisterFunction<RemoveContextMenuFunction>();
  RegisterFunction<RemoveAllContextMenusFunction>();

  // Omnibox.
  RegisterFunction<OmniboxSendSuggestionsFunction>();
  RegisterFunction<OmniboxSetDefaultSuggestionFunction>();

  // Proxies.
  RegisterFunction<SetProxySettingsFunction>();
  RegisterFunction<GetProxySettingsFunction>();

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

  // Management.
  RegisterFunction<GetAllExtensionsFunction>();
  RegisterFunction<GetExtensionByIdFunction>();
  RegisterFunction<LaunchAppFunction>();
  RegisterFunction<SetEnabledFunction>();
  RegisterFunction<UninstallFunction>();

  // Extension module.
  RegisterFunction<SetUpdateUrlDataFunction>();

  // WebstorePrivate.
  RegisterFunction<GetBrowserLoginFunction>();
  RegisterFunction<GetStoreLoginFunction>();
  RegisterFunction<SetStoreLoginFunction>();
  RegisterFunction<PromptBrowserLoginFunction>();
  RegisterFunction<BeginInstallFunction>();
  RegisterFunction<CompleteInstallFunction>();

  // WebRequest.
  RegisterFunction<WebRequestAddEventListener>();

  // Preferences.
  RegisterFunction<GetPreferenceFunction>();
  RegisterFunction<SetPreferenceFunction>();
  RegisterFunction<ClearPreferenceFunction>();
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

ExtensionFunctionDispatcher* ExtensionFunctionDispatcher::Create(
    RenderViewHost* render_view_host,
    Delegate* delegate,
    const GURL& url) {
  ExtensionService* service =
      render_view_host->process()->profile()->GetExtensionService();
  DCHECK(service);

  if (!service->ExtensionBindingsAllowed(url))
    return NULL;

  const Extension* extension = service->GetExtensionByURL(url);
  if (!extension)
    extension = service->GetExtensionByWebExtent(url);

  if (extension)
    return new ExtensionFunctionDispatcher(render_view_host, delegate,
                                           extension, url);
  else
    return NULL;
}

ExtensionFunctionDispatcher::ExtensionFunctionDispatcher(
    RenderViewHost* render_view_host,
    Delegate* delegate,
    const Extension* extension,
    const GURL& url)
  : profile_(render_view_host->process()->profile()),
    render_view_host_(render_view_host),
    delegate_(delegate),
    url_(url),
    extension_id_(extension->id()),
    ALLOW_THIS_IN_INITIALIZER_LIST(peer_(new Peer(this))) {
  // TODO(erikkay) should we do something for these errors in Release?
  DCHECK(extension);
  DCHECK(url.SchemeIs(chrome::kExtensionScheme) ||
         extension->location() == Extension::COMPONENT);

  // Notify the ExtensionProcessManager that the view was created.
  ExtensionProcessManager* epm = profile()->GetExtensionProcessManager();
  epm->RegisterExtensionProcess(extension_id(),
                                render_view_host->process()->id());

  // If the extension has permission to load chrome://favicon/ resources we need
  // to make sure that the FavIconSource is registered with the
  // ChromeURLDataManager.
  if (extension->HasHostPermission(GURL(chrome::kChromeUIFavIconURL))) {
    FavIconSource* favicon_source = new FavIconSource(profile_);
    profile_->GetChromeURLDataManager()->AddDataSource(favicon_source);
  }

  // Update the extension permissions. Doing this each time we create an EFD
  // ensures that new processes are informed of permissions for newly installed
  // extensions.
  render_view_host->Send(new ViewMsg_Extension_SetAPIPermissions(
      extension->id(), extension->api_permissions()));
  render_view_host->Send(new ViewMsg_Extension_SetHostPermissions(
      extension->url(), extension->host_permissions()));

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_FUNCTION_DISPATCHER_CREATED,
      Source<Profile>(profile_),
      Details<ExtensionFunctionDispatcher>(this));
}

ExtensionFunctionDispatcher::~ExtensionFunctionDispatcher() {
  peer_->dispatcher_ = NULL;

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_FUNCTION_DISPATCHER_DESTROYED,
      Source<Profile>(profile_),
      Details<ExtensionFunctionDispatcher>(this));
}

Browser* ExtensionFunctionDispatcher::GetCurrentBrowser(
    bool include_incognito) {
  Browser* browser = delegate_->GetBrowser();

  // If the delegate has an associated browser, that is always the right answer.
  if (browser)
    return browser;

  // Otherwise, try to default to a reasonable browser. If |include_incognito|
  // is true, we will also search browsers in the incognito version of this
  // profile. Note that the profile may already be incognito, in which case
  // we will search the incognito version only, regardless of the value of
  // |include_incognito|.
  Profile* profile = render_view_host()->process()->profile();
  browser = BrowserList::FindBrowserWithType(profile, Browser::TYPE_NORMAL,
                                             include_incognito);

  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a background_page onload chrome.tabs api call can make it into here
  // before the browser is sufficiently initialized to return here.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return browser;
}

void ExtensionFunctionDispatcher::HandleRequest(
    const ViewHostMsg_DomMessage_Params& params) {
  scoped_refptr<ExtensionFunction> function(
      FactoryRegistry::GetInstance()->NewFunction(params.name));
  function->set_dispatcher_peer(peer_);
  function->set_profile(profile_);
  function->set_extension_id(extension_id());
  function->SetArgs(&params.arguments);
  function->set_source_url(params.source_url);
  function->set_request_id(params.request_id);
  function->set_has_callback(params.has_callback);
  function->set_user_gesture(params.user_gesture);
  ExtensionService* service = profile()->GetExtensionService();
  DCHECK(service);
  const Extension* extension = service->GetExtensionById(extension_id(), false);
  DCHECK(extension);
  function->set_include_incognito(service->CanCrossIncognito(extension));

  if (!service->ExtensionBindingsAllowed(function->source_url()) ||
      !extension->HasApiPermission(function->name())) {
    render_view_host_->BlockExtensionRequest(function->request_id());
    return;
  }

  ExtensionsQuotaService* quota = service->quota_service();
  if (quota->Assess(extension_id(), function, &params.arguments,
                    base::TimeTicks::Now())) {
    // See crbug.com/39178.
    ExternalProtocolHandler::PermitLaunchUrl();

    function->Run();
  } else {
    render_view_host_->SendExtensionResponse(function->request_id(), false,
        std::string(), QuotaLimitHeuristic::kGenericOverQuotaError);
  }
}

void ExtensionFunctionDispatcher::SendResponse(ExtensionFunction* function,
                                               bool success) {
  render_view_host_->SendExtensionResponse(function->request_id(), success,
      function->GetResult(), function->GetError());
}

void ExtensionFunctionDispatcher::HandleBadMessage(ExtensionFunction* api) {
  LOG(ERROR) << "bad extension message " <<
                api->name() <<
                " : terminating renderer.";
  if (RenderProcessHost::run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  } else {
    NOTREACHED();
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_EFD"));
    base::KillProcess(render_view_host_->process()->GetHandle(),
                      ResultCodes::KILLED_BAD_MESSAGE, false);
  }
}

Profile* ExtensionFunctionDispatcher::profile() {
  return profile_;
}
