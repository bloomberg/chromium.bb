// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_dispatcher.h"

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/result_codes.h"
#include "extensions/common/extension_api.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "webkit/common/resource_type.h"

using extensions::Extension;
using extensions::ExtensionAPI;
using extensions::Feature;
using content::RenderViewHost;

namespace {

void LogSuccess(const std::string& extension_id,
                const std::string& api_name,
                scoped_ptr<base::ListValue> args,
                Profile* profile) {
  // The ActivityLog can only be accessed from the main (UI) thread.  If we're
  // running on the wrong thread, re-dispatch from the main thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&LogSuccess,
                                       extension_id,
                                       api_name,
                                       base::Passed(&args),
                                       profile));
  } else {
    extensions::ActivityLog* activity_log =
        extensions::ActivityLog::GetInstance(profile);
    scoped_refptr<extensions::Action> action =
        new extensions::Action(extension_id,
                               base::Time::Now(),
                               extensions::Action::ACTION_API_CALL,
                               api_name);
    action->set_args(args.Pass());
    activity_log->LogAction(action);
  }
}

// Separate copy of ExtensionAPI used for IO thread extension functions. We need
// this because ExtensionAPI has mutable data. It should be possible to remove
// this once all the extension APIs are updated to the feature system.
struct Static {
  Static()
      : api(extensions::ExtensionAPI::CreateWithDefaultConfiguration()) {
  }
  scoped_ptr<extensions::ExtensionAPI> api;
};
base::LazyInstance<Static> g_global_io_data = LAZY_INSTANCE_INITIALIZER;

// Kills the specified process because it sends us a malformed message.
void KillBadMessageSender(base::ProcessHandle process) {
  NOTREACHED();
  content::RecordAction(content::UserMetricsAction("BadMessageTerminate_EFD"));
  if (process)
    base::KillProcess(process, content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
}

void CommonResponseCallback(IPC::Sender* ipc_sender,
                            int routing_id,
                            base::ProcessHandle peer_process,
                            int request_id,
                            ExtensionFunction::ResponseType type,
                            const base::ListValue& results,
                            const std::string& error) {
  DCHECK(ipc_sender);

  if (type == ExtensionFunction::BAD_MESSAGE) {
    // The renderer has done validation before sending extension api requests.
    // Therefore, we should never receive a request that is invalid in a way
    // that JSON validation in the renderer should have caught. It could be an
    // attacker trying to exploit the browser, so we crash the renderer instead.
    LOG(ERROR) <<
        "Terminating renderer because of malformed extension message.";
    if (content::RenderProcessHost::run_renderer_in_process()) {
      // In single process mode it is better if we don't suicide but just crash.
      CHECK(false);
    } else {
      KillBadMessageSender(peer_process);
    }

    return;
  }

  ipc_sender->Send(new ExtensionMsg_Response(
      routing_id, request_id, type == ExtensionFunction::SUCCEEDED, results,
      error));
}

void IOThreadResponseCallback(
    const base::WeakPtr<ChromeRenderMessageFilter>& ipc_sender,
    int routing_id,
    int request_id,
    ExtensionFunction::ResponseType type,
    const base::ListValue& results,
    const std::string& error) {
  if (!ipc_sender.get())
    return;

  CommonResponseCallback(ipc_sender.get(),
                         routing_id,
                         ipc_sender->PeerHandle(),
                         request_id,
                         type,
                         results,
                         error);
}

}  // namespace

class ExtensionFunctionDispatcher::UIThreadResponseCallbackWrapper
    : public content::WebContentsObserver {
 public:
  UIThreadResponseCallbackWrapper(
      const base::WeakPtr<ExtensionFunctionDispatcher>& dispatcher,
      RenderViewHost* render_view_host)
      : content::WebContentsObserver(
            content::WebContents::FromRenderViewHost(render_view_host)),
        dispatcher_(dispatcher),
        render_view_host_(render_view_host),
        weak_ptr_factory_(this) {
  }

  virtual ~UIThreadResponseCallbackWrapper() {
  }

  // content::WebContentsObserver overrides.
  virtual void RenderViewDeleted(
      RenderViewHost* render_view_host) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (render_view_host != render_view_host_)
      return;

    if (dispatcher_.get()) {
      dispatcher_->ui_thread_response_callback_wrappers_
          .erase(render_view_host);
    }

    delete this;
  }

  ExtensionFunction::ResponseCallback CreateCallback(int request_id) {
    return base::Bind(
        &UIThreadResponseCallbackWrapper::OnExtensionFunctionCompleted,
        weak_ptr_factory_.GetWeakPtr(),
        request_id);
  }

 private:
  void OnExtensionFunctionCompleted(int request_id,
                                    ExtensionFunction::ResponseType type,
                                    const base::ListValue& results,
                                    const std::string& error) {
    CommonResponseCallback(
        render_view_host_, render_view_host_->GetRoutingID(),
        render_view_host_->GetProcess()->GetHandle(), request_id, type,
        results, error);
  }

  base::WeakPtr<ExtensionFunctionDispatcher> dispatcher_;
  content::RenderViewHost* render_view_host_;
  base::WeakPtrFactory<UIThreadResponseCallbackWrapper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadResponseCallbackWrapper);
};

extensions::WindowController*
ExtensionFunctionDispatcher::Delegate::GetExtensionWindowController()
    const {
  return NULL;
}

content::WebContents*
ExtensionFunctionDispatcher::Delegate::GetAssociatedWebContents() const {
  return NULL;
}

content::WebContents*
ExtensionFunctionDispatcher::Delegate::GetVisibleWebContents() const {
  return GetAssociatedWebContents();
}

void ExtensionFunctionDispatcher::GetAllFunctionNames(
    std::vector<std::string>* names) {
  ExtensionFunctionRegistry::GetInstance()->GetAllNames(names);
}

bool ExtensionFunctionDispatcher::OverrideFunction(
    const std::string& name, ExtensionFunctionFactory factory) {
  return ExtensionFunctionRegistry::GetInstance()->OverrideFunction(name,
                                                                    factory);
}

void ExtensionFunctionDispatcher::ResetFunctions() {
  ExtensionFunctionRegistry::GetInstance()->ResetFunctions();
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
  Profile* profile_cast = static_cast<Profile*>(profile);

  ExtensionFunction::ResponseCallback callback(
      base::Bind(&IOThreadResponseCallback, ipc_sender, routing_id,
                 params.request_id));

  scoped_refptr<ExtensionFunction> function(
      CreateExtensionFunction(params, extension, render_process_id,
                              extension_info_map->process_map(),
                              g_global_io_data.Get().api.get(),
                              profile, callback));
  scoped_ptr<ListValue> args(params.arguments.DeepCopy());

  if (!function.get())
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

  if (!CheckPermissions(function.get(), extension, params, callback))
    return;

  ExtensionsQuotaService* quota = extension_info_map->GetQuotaService();
  std::string violation_error = quota->Assess(extension->id(),
                                              function.get(),
                                              &params.arguments,
                                              base::TimeTicks::Now());
  if (violation_error.empty()) {
    LogSuccess(extension->id(),
               params.name,
               args.Pass(),
               profile_cast);
    function->Run();
  } else {
    function->OnQuotaExceeded(violation_error);
  }
}

ExtensionFunctionDispatcher::ExtensionFunctionDispatcher(Profile* profile,
                                                         Delegate* delegate)
  : profile_(profile),
    delegate_(delegate) {
}

ExtensionFunctionDispatcher::~ExtensionFunctionDispatcher() {
}

void ExtensionFunctionDispatcher::Dispatch(
    const ExtensionHostMsg_Request_Params& params,
    RenderViewHost* render_view_host) {
  UIThreadResponseCallbackWrapperMap::const_iterator
      iter = ui_thread_response_callback_wrappers_.find(render_view_host);
  UIThreadResponseCallbackWrapper* callback_wrapper = NULL;
  if (iter == ui_thread_response_callback_wrappers_.end()) {
    callback_wrapper = new UIThreadResponseCallbackWrapper(AsWeakPtr(),
                                                           render_view_host);
    ui_thread_response_callback_wrappers_[render_view_host] = callback_wrapper;
  } else {
    callback_wrapper = iter->second;
  }

  DispatchWithCallback(params, render_view_host,
                       callback_wrapper->CreateCallback(params.request_id));
}

void ExtensionFunctionDispatcher::DispatchWithCallback(
    const ExtensionHostMsg_Request_Params& params,
    RenderViewHost* render_view_host,
    const ExtensionFunction::ResponseCallback& callback) {
  // TODO(yzshen): There is some shared logic between this method and
  // DispatchOnIOThread(). It is nice to deduplicate.
  ExtensionService* service = profile()->GetExtensionService();
  ExtensionProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile())->process_manager();
  extensions::ProcessMap* process_map = service->process_map();
  if (!service || !process_map)
    return;

  const Extension* extension = service->extensions()->GetByID(
      params.extension_id);
  if (!extension)
    extension = service->extensions()->GetHostedAppByURL(params.source_url);

  scoped_refptr<ExtensionFunction> function(
      CreateExtensionFunction(params, extension,
                              render_view_host->GetProcess()->GetID(),
                              *(service->process_map()),
                              extensions::ExtensionAPI::GetSharedInstance(),
                              profile(), callback));
  scoped_ptr<ListValue> args(params.arguments.DeepCopy());

  if (!function.get())
    return;

  UIThreadExtensionFunction* function_ui =
      function->AsUIThreadExtensionFunction();
  if (!function_ui) {
    NOTREACHED();
    return;
  }
  function_ui->SetRenderViewHost(render_view_host);
  function_ui->set_dispatcher(AsWeakPtr());
  function_ui->set_context(profile_);
  function->set_include_incognito(extension_util::CanCrossIncognito(extension,
                                                                    service));

  if (!CheckPermissions(function.get(), extension, params, callback))
    return;

  ExtensionsQuotaService* quota = service->quota_service();
  std::string violation_error = quota->Assess(extension->id(),
                                              function.get(),
                                              &params.arguments,
                                              base::TimeTicks::Now());
  if (violation_error.empty()) {
    // See crbug.com/39178.
    ExternalProtocolHandler::PermitLaunchUrl();
    LogSuccess(extension->id(), params.name, args.Pass(), profile());
    function->Run();
  } else {
    function->OnQuotaExceeded(violation_error);
  }

  // Note: do not access |this| after this point. We may have been deleted
  // if function->Run() ended up closing the tab that owns us.

  // Check if extension was uninstalled by management.uninstall.
  if (!service->extensions()->GetByID(params.extension_id))
    return;

  // We only adjust the keepalive count for UIThreadExtensionFunction for
  // now, largely for simplicity's sake. This is OK because currently, only
  // the webRequest API uses IOThreadExtensionFunction, and that API is not
  // compatible with lazy background pages.
  process_manager->IncrementLazyKeepaliveCount(extension);
}

void ExtensionFunctionDispatcher::OnExtensionFunctionCompleted(
    const Extension* extension) {
  extensions::ExtensionSystem::Get(profile())->process_manager()->
      DecrementLazyKeepaliveCount(extension);
}

// static
bool ExtensionFunctionDispatcher::CheckPermissions(
    ExtensionFunction* function,
    const Extension* extension,
    const ExtensionHostMsg_Request_Params& params,
    const ExtensionFunction::ResponseCallback& callback) {
  if (!function->HasPermission()) {
    LOG(ERROR) << "Extension " << extension->id() << " does not have "
               << "permission to function: " << params.name;
    SendAccessDenied(callback);
    return false;
  }
  return true;
}

namespace {

// Only COMPONENT hosted apps may call extension APIs, and they are limited
// to just the permissions they explicitly request. They should not have access
// to extension APIs like eg chrome.runtime, chrome.windows, etc. that normally
// are available without permission.
// TODO(mpcomplete): move this to ExtensionFunction::HasPermission (or remove
// it altogether).
bool AllowHostedAppAPICall(const Extension& extension,
                           const GURL& source_url,
                           const std::string& function_name) {
  if (extension.location() != extensions::Manifest::COMPONENT)
    return false;

  if (!extension.web_extent().MatchesURL(source_url))
    return false;

  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          function_name, &extension, Feature::BLESSED_EXTENSION_CONTEXT,
          source_url);
  return availability.is_available();
}

}  // namespace


// static
ExtensionFunction* ExtensionFunctionDispatcher::CreateExtensionFunction(
    const ExtensionHostMsg_Request_Params& params,
    const Extension* extension,
    int requesting_process_id,
    const extensions::ProcessMap& process_map,
    extensions::ExtensionAPI* api,
    void* profile,
    const ExtensionFunction::ResponseCallback& callback) {
  if (!extension) {
    LOG(ERROR) << "Specified extension does not exist.";
    SendAccessDenied(callback);
    return NULL;
  }

  // Most hosted apps can't call APIs.
  bool allowed = true;
  if (extension->is_hosted_app())
    allowed = AllowHostedAppAPICall(*extension, params.source_url, params.name);

  // Privileged APIs can only be called from the process the extension
  // is running in.
  if (allowed && api->IsPrivileged(params.name))
    allowed = process_map.Contains(extension->id(), requesting_process_id);

  if (!allowed) {
    LOG(ERROR) << "Extension API call disallowed - name:" << params.name
               << " pid:" << requesting_process_id
               << " from URL " << params.source_url.spec();
    SendAccessDenied(callback);
    return NULL;
  }

  ExtensionFunction* function =
      ExtensionFunctionRegistry::GetInstance()->NewFunction(params.name);
  if (!function) {
    LOG(ERROR) << "Unknown Extension API - " << params.name;
    SendAccessDenied(callback);
    return NULL;
  }

  function->SetArgs(&params.arguments);
  function->set_source_url(params.source_url);
  function->set_request_id(params.request_id);
  function->set_has_callback(params.has_callback);
  function->set_user_gesture(params.user_gesture);
  function->set_extension(extension);
  function->set_profile_id(profile);
  function->set_response_callback(callback);

  return function;
}

// static
void ExtensionFunctionDispatcher::SendAccessDenied(
    const ExtensionFunction::ResponseCallback& callback) {
  ListValue empty_list;
  callback.Run(ExtensionFunction::FAILED, empty_list,
               "Access to extension API denied.");
}
