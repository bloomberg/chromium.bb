// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_dispatcher.h"

#include <map>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/process_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/glue/resource_type.h"

using extensions::Extension;
using extensions::ExtensionAPI;
using content::RenderViewHost;
using WebKit::WebSecurityOrigin;

namespace {

const char kAccessDenied[] = "access denied";
const char kQuotaExceeded[] = "quota exceeded";

void LogSuccess(const Extension* extension,
                const std::string& api_name,
                scoped_ptr<ListValue> args,
                Profile* profile) {
  // The ActivityLog can only be accessed from the main (UI) thread.  If we're
  // running on the wrong thread, re-dispatch from the main thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&LogSuccess,
                                       extension,
                                       api_name,
                                       Passed(args.Pass()),
                                       profile));
  } else {
    extensions::ActivityLog* activity_log =
        extensions::ActivityLog::GetInstance(profile);
    activity_log->LogAPIAction(extension, api_name, args.get(), "");
  }
}

void LogFailure(const Extension* extension,
                const std::string& api_name,
                scoped_ptr<ListValue> args,
                const char* reason,
                Profile* profile) {
  // The ActivityLog can only be accessed from the main (UI) thread.  If we're
  // running on the wrong thread, re-dispatch from the main thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&LogFailure,
                                       extension,
                                       api_name,
                                       Passed(args.Pass()),
                                       reason,
                                       profile));
  } else {
    extensions::ActivityLog* activity_log =
        extensions::ActivityLog::GetInstance(profile);
    activity_log->LogBlockedAction(extension,
                                   api_name,
                                   args.get(),
                                   reason,
                                   "");
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

}  // namespace

extensions::WindowController*
ExtensionFunctionDispatcher::Delegate::GetExtensionWindowController()
    const {
  return NULL;
}

content::WebContents*
ExtensionFunctionDispatcher::Delegate::GetAssociatedWebContents() const {
  return NULL;
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
  scoped_refptr<ExtensionFunction> function(
      CreateExtensionFunction(params, extension, render_process_id,
                              extension_info_map->process_map(),
                              g_global_io_data.Get().api.get(),
                              profile,
                              ipc_sender, NULL, routing_id));
  scoped_ptr<ListValue> args(params.arguments.DeepCopy());

  if (!function) {
    LogFailure(extension,
               params.name,
               args.Pass(),
               kAccessDenied,
               profile_cast);
    return;
  }

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

  if (!CheckPermissions(function, extension, params, ipc_sender, routing_id)) {
    LogFailure(extension,
               params.name,
               args.Pass(),
               kAccessDenied,
               profile_cast);
    return;
  }

  ExtensionsQuotaService* quota = extension_info_map->GetQuotaService();
  std::string violation_error = quota->Assess(extension->id(),
                                              function,
                                              &params.arguments,
                                              base::TimeTicks::Now());
  if (violation_error.empty()) {
    LogSuccess(extension,
               params.name,
               args.Pass(),
               profile_cast);
    function->Run();
  } else {
    LogFailure(extension,
               params.name,
               args.Pass(),
               kQuotaExceeded,
               profile_cast);
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
  ExtensionService* service = profile()->GetExtensionService();
  ExtensionProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile())->process_manager();
  extensions::ProcessMap* process_map = service->process_map();
  if (!service || !process_map)
    return;

  const Extension* extension = service->extensions()->GetByID(
      params.extension_id);
  if (!extension)
    extension = service->extensions()->GetHostedAppByURL(ExtensionURLInfo(
        WebSecurityOrigin::createFromString(params.source_origin),
        params.source_url));

  scoped_refptr<ExtensionFunction> function(
      CreateExtensionFunction(params, extension,
                              render_view_host->GetProcess()->GetID(),
                              *(service->process_map()),
                              extensions::ExtensionAPI::GetSharedInstance(),
                              profile(), render_view_host, render_view_host,
                              render_view_host->GetRoutingID()));
  scoped_ptr<ListValue> args(params.arguments.DeepCopy());

  if (!function) {
    LogFailure(extension,
               params.name,
               args.Pass(),
               kAccessDenied,
               profile());
    return;
  }

  UIThreadExtensionFunction* function_ui =
      function->AsUIThreadExtensionFunction();
  if (!function_ui) {
    NOTREACHED();
    return;
  }
  function_ui->set_dispatcher(AsWeakPtr());
  function_ui->set_profile(profile_);
  function->set_include_incognito(service->CanCrossIncognito(extension));

  if (!CheckPermissions(function, extension, params, render_view_host,
                        render_view_host->GetRoutingID())) {
    LogFailure(extension,
               params.name,
               args.Pass(),
               kAccessDenied,
               profile());
    return;
  }

  ExtensionsQuotaService* quota = service->quota_service();
  std::string violation_error = quota->Assess(extension->id(),
                                              function,
                                              &params.arguments,
                                              base::TimeTicks::Now());
  if (violation_error.empty()) {
    // See crbug.com/39178.
    ExternalProtocolHandler::PermitLaunchUrl();
    LogSuccess(extension, params.name, args.Pass(), profile());
    function->Run();
  } else {
    LogFailure(extension,
               params.name,
               args.Pass(),
               kQuotaExceeded,
               profile());
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
    IPC::Sender* ipc_sender,
    int routing_id) {
  if (!function->HasPermission()) {
    LOG(ERROR) << "Extension " << extension->id() << " does not have "
               << "permission to function: " << params.name;
    SendAccessDenied(ipc_sender, routing_id, params.request_id);
    return false;
  }
  return true;
}

// static
ExtensionFunction* ExtensionFunctionDispatcher::CreateExtensionFunction(
    const ExtensionHostMsg_Request_Params& params,
    const Extension* extension,
    int requesting_process_id,
    const extensions::ProcessMap& process_map,
    extensions::ExtensionAPI* api,
    void* profile,
    IPC::Sender* ipc_sender,
    RenderViewHost* render_view_host,
    int routing_id) {
  if (!extension) {
    LOG(ERROR) << "Specified extension does not exist.";
    SendAccessDenied(ipc_sender, routing_id, params.request_id);
    return NULL;
  }

  if (api->IsPrivileged(params.name) &&
      !process_map.Contains(extension->id(), requesting_process_id)) {
    LOG(ERROR) << "Extension API called from incorrect process "
               << requesting_process_id
               << " from URL " << params.source_url.spec();
    SendAccessDenied(ipc_sender, routing_id, params.request_id);
    return NULL;
  }

  ExtensionFunction* function =
      ExtensionFunctionRegistry::GetInstance()->NewFunction(params.name);
  function->SetArgs(&params.arguments);
  function->set_source_url(params.source_url);
  function->set_request_id(params.request_id);
  function->set_has_callback(params.has_callback);
  function->set_user_gesture(params.user_gesture);
  function->set_extension(extension);
  function->set_profile_id(profile);

  UIThreadExtensionFunction* function_ui =
      function->AsUIThreadExtensionFunction();
  if (function_ui) {
    function_ui->SetRenderViewHost(render_view_host);
  }

  return function;
}

// static
void ExtensionFunctionDispatcher::SendAccessDenied(
    IPC::Sender* ipc_sender, int routing_id, int request_id) {
  ListValue empty_list;
  ipc_sender->Send(new ExtensionMsg_Response(
      routing_id, request_id, false, empty_list,
      "Access to extension API denied."));
}
