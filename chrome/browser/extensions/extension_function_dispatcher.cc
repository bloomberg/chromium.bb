// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_dispatcher.h"

#include <map>

#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/process_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_activity_log.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_service.h"
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
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/resource_type.h"

using extensions::ExtensionAPI;
using content::RenderViewHost;
using WebKit::WebSecurityOrigin;

namespace {

const char kAccessDenied[] = "access denied";
const char kQuotaExceeded[] = "quota exceeded";

void LogSuccess(const Extension* extension,
                const ExtensionHostMsg_Request_Params& params) {
  ExtensionActivityLog* extension_activity_log =
      ExtensionActivityLog::GetInstance();
  if (extension_activity_log->HasObservers(extension)) {
    std::string call_signature = params.name + "(";
    ListValue::const_iterator it = params.arguments.begin();
    for (; it != params.arguments.end(); ++it) {
      std::string arg;
      JSONStringValueSerializer serializer(&arg);
      if (serializer.SerializeAndOmitBinaryValues(**it)) {
        if (it != params.arguments.begin())
          call_signature += ", ";
        call_signature += arg;
      }
    }
    call_signature += ")";

    extension_activity_log->Log(
        extension,
        ExtensionActivityLog::ACTIVITY_EXTENSION_API_CALL,
        call_signature);
  }
}

void LogFailure(const Extension* extension,
                const std::string& func_name,
                const char* reason) {
  ExtensionActivityLog* extension_activity_log =
      ExtensionActivityLog::GetInstance();
  if (extension_activity_log->HasObservers(extension)) {
    extension_activity_log->Log(
        extension,
        ExtensionActivityLog::ACTIVITY_EXTENSION_API_BLOCK,
        func_name + ": " + reason);
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

  scoped_refptr<ExtensionFunction> function(
      CreateExtensionFunction(params, extension, render_process_id,
                              extension_info_map->process_map(),
                              g_global_io_data.Get().api.get(),
                              profile,
                              ipc_sender, routing_id));
  if (!function) {
    LogFailure(extension, params.name, kAccessDenied);
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

  ExtensionsQuotaService* quota = extension_info_map->GetQuotaService();
  if (quota->Assess(extension->id(), function, &params.arguments,
                    base::TimeTicks::Now())) {
    function->Run();
    LogSuccess(extension, params);
  } else {
    function->OnQuotaExceeded();
    LogFailure(extension, params.name, kQuotaExceeded);
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
                              profile(), render_view_host,
                              render_view_host->GetRoutingID()));
  if (!function) {
    LogFailure(extension, params.name, kAccessDenied);
    return;
  }

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
    LogSuccess(extension, params);
  } else {
    function->OnQuotaExceeded();
    LogFailure(extension, params.name, kQuotaExceeded);
  }

  // Check if extension was uninstalled by management.uninstall.
  if (!service->extensions()->GetByID(params.extension_id))
    return;

  // We only adjust the keepalive count for UIThreadExtensionFunction for
  // now, largely for simplicity's sake. This is OK because currently, only
  // the webRequest API uses IOThreadExtensionFunction, and that API is not
  // compatible with lazy background pages.
  profile()->GetExtensionProcessManager()->IncrementLazyKeepaliveCount(
      extension);
}

void ExtensionFunctionDispatcher::OnExtensionFunctionCompleted(
    const Extension* extension) {
  profile()->GetExtensionProcessManager()->DecrementLazyKeepaliveCount(
      extension);
}

// static
ExtensionFunction* ExtensionFunctionDispatcher::CreateExtensionFunction(
    const ExtensionHostMsg_Request_Params& params,
    const Extension* extension,
    int requesting_process_id,
    const extensions::ProcessMap& process_map,
    extensions::ExtensionAPI* api,
    void* profile,
    IPC::Message::Sender* ipc_sender,
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

  if (!extension->HasAPIPermission(params.name)) {
    LOG(ERROR) << "Extension " << extension->id() << " does not have "
               << "permission to function: " << params.name;
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
  return function;
}

// static
void ExtensionFunctionDispatcher::SendAccessDenied(
    IPC::Message::Sender* ipc_sender, int routing_id, int request_id) {
  ListValue empty_list;
  ipc_sender->Send(new ExtensionMsg_Response(
      routing_id, request_id, false, empty_list,
      "Access to extension API denied."));
}
