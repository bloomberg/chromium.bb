// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_dispatcher.h"

#include "base/process_util.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/extensions/execute_code_in_tab_function.h"
#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"
#include "chrome/browser/extensions/extension_browser_actions_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_history_api.h"
#include "chrome/browser/extensions/extension_i18n_api.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_page_actions_module.h"
#include "chrome/browser/extensions/extension_page_actions_module_constants.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/browser/extensions/extension_toolstrip_api.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/url_constants.h"

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
  static FactoryRegistry* instance();
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

FactoryRegistry* FactoryRegistry::instance() {
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

  // Browser Actions.
  RegisterFunction<BrowserActionSetIconFunction>();
  RegisterFunction<BrowserActionSetTitleFunction>();
  RegisterFunction<BrowserActionSetBadgeTextFunction>();
  RegisterFunction<BrowserActionSetBadgeBackgroundColorFunction>();

  // Bookmarks.
  RegisterFunction<GetBookmarksFunction>();
  RegisterFunction<GetBookmarkChildrenFunction>();
  RegisterFunction<GetBookmarkTreeFunction>();
  RegisterFunction<SearchBookmarksFunction>();
  RegisterFunction<RemoveBookmarkFunction>();
  RegisterFunction<RemoveTreeBookmarkFunction>();
  RegisterFunction<CreateBookmarkFunction>();
  RegisterFunction<MoveBookmarkFunction>();
  RegisterFunction<UpdateBookmarkFunction>();

  // History
  RegisterFunction<AddUrlHistoryFunction>();
  RegisterFunction<DeleteAllHistoryFunction>();
  RegisterFunction<DeleteRangeHistoryFunction>();
  RegisterFunction<DeleteUrlHistoryFunction>();
  RegisterFunction<GetVisitsHistoryFunction>();
  RegisterFunction<SearchHistoryFunction>();

  // Toolstrips.
  RegisterFunction<ToolstripExpandFunction>();
  RegisterFunction<ToolstripCollapseFunction>();

  // I18N.
  RegisterFunction<GetAcceptLanguagesFunction>();

  // Test.
  RegisterFunction<ExtensionTestPassFunction>();
  RegisterFunction<ExtensionTestFailFunction>();
  RegisterFunction<ExtensionTestLogFunction>();
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
  FactoryRegistry::instance()->GetAllNames(names);
}

bool ExtensionFunctionDispatcher::OverrideFunction(
    const std::string& name, ExtensionFunctionFactory factory) {
  return FactoryRegistry::instance()->OverrideFunction(name, factory);
}

void ExtensionFunctionDispatcher::ResetFunctions() {
  FactoryRegistry::instance()->ResetFunctions();
}

std::set<ExtensionFunctionDispatcher*>*
    ExtensionFunctionDispatcher::all_instances() {
  static std::set<ExtensionFunctionDispatcher*> instances;
  return &instances;
}

ExtensionFunctionDispatcher::ExtensionFunctionDispatcher(
    RenderViewHost* render_view_host,
    Delegate* delegate,
    const GURL& url)
  : render_view_host_(render_view_host),
    delegate_(delegate),
    url_(url),
    ALLOW_THIS_IN_INITIALIZER_LIST(peer_(new Peer(this))) {
  // TODO(erikkay) should we do something for these errors in Release?
  DCHECK(url.SchemeIs(chrome::kExtensionScheme));

  Extension* extension =
      profile()->GetExtensionsService()->GetExtensionByURL(url);
  DCHECK(extension);

  all_instances()->insert(this);

  // Notify the ExtensionProcessManager that the view was created.
  ExtensionProcessManager* epm = profile()->GetExtensionProcessManager();
  epm->RegisterExtensionProcess(extension_id(),
                                render_view_host->process()->id());

  // Update the extension permissions. Doing this each time we create an EFD
  // ensures that new processes are informed of permissions for newly installed
  // extensions.
  render_view_host->Send(new ViewMsg_Extension_SetAPIPermissions(
      extension->id(), extension->api_permissions()));
  render_view_host->Send(new ViewMsg_Extension_SetHostPermissions(
      extension->url(), extension->host_permissions()));
}

ExtensionFunctionDispatcher::~ExtensionFunctionDispatcher() {
  all_instances()->erase(this);
  peer_->dispatcher_ = NULL;
}

Browser* ExtensionFunctionDispatcher::GetBrowser() {
  return delegate_->GetBrowser();
}

ExtensionHost* ExtensionFunctionDispatcher::GetExtensionHost() {
  return delegate_->GetExtensionHost();
}

Extension* ExtensionFunctionDispatcher::GetExtension() {
  ExtensionsService* service = profile()->GetExtensionsService();
  DCHECK(service);

  Extension* extension = service->GetExtensionById(extension_id());
  DCHECK(extension);

  return extension;
}

void ExtensionFunctionDispatcher::HandleRequest(const std::string& name,
                                                const Value* args,
                                                int request_id,
                                                bool has_callback) {
  scoped_refptr<ExtensionFunction> function(
      FactoryRegistry::instance()->NewFunction(name));
  function->set_dispatcher_peer(peer_);
  function->SetArgs(args);
  function->set_request_id(request_id);
  function->set_has_callback(has_callback);
  function->Run();
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
    base::KillProcess(render_view_host_->process()->process().handle(),
                      ResultCodes::KILLED_BAD_MESSAGE, false);
  }
}

Profile* ExtensionFunctionDispatcher::profile() {
  return render_view_host_->process()->profile();
}
