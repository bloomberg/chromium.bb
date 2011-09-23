// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_dispatcher.h"

#include "base/command_line.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_app_bindings.h"
#include "chrome/renderer/extensions/chrome_webstore_bindings.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_bindings_context.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/js_only_v8_extensions.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/renderer/render_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "v8/include/v8.h"

namespace {
static const double kInitialExtensionIdleHandlerDelayS = 5.0 /* seconds */;
static const int64 kMaxExtensionIdleHandlerDelayS = 5*60 /* seconds */;
}

using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;

ExtensionDispatcher::ExtensionDispatcher()
    : is_webkit_initialized_(false) {
  std::string type_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kProcessType);
  is_extension_process_ = type_str == switches::kExtensionProcess ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);

  if (is_extension_process_) {
    RenderThread::current()->set_idle_notification_delay_in_s(
        kInitialExtensionIdleHandlerDelayS);
  }

  user_script_slave_.reset(new UserScriptSlave(&extensions_));
}

ExtensionDispatcher::~ExtensionDispatcher() {
}

bool ExtensionDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionDispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnDeliverMessage)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetFunctionNames, OnSetFunctionNames)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Loaded, OnLoaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Unloaded, OnUnloaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetScriptingWhitelist,
                        OnSetScriptingWhitelist)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateExtension, OnActivateExtension)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateApplication, OnActivateApplication)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdatePermissions, OnUpdatePermissions)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateUserScripts, OnUpdateUserScripts)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ExtensionDispatcher::WebKitInitialized() {
  // For extensions, we want to ensure we call the IdleHandler every so often,
  // even if the extension keeps up activity.
  if (is_extension_process_) {
    forced_idle_timer_.Start(FROM_HERE,
        base::TimeDelta::FromSeconds(kMaxExtensionIdleHandlerDelayS),
        RenderThread::current(), &RenderThread::IdleHandler);
  }

  RegisterExtension(extensions_v8::ChromeAppExtension::Get(this), false);
  RegisterExtension(ChromeWebstoreExtension::Get(), false);

  // Add v8 extensions related to chrome extensions.
  RegisterExtension(ExtensionProcessBindings::Get(this), true);
  RegisterExtension(JsonSchemaJsV8Extension::Get(), true);
  RegisterExtension(EventBindings::Get(this), true);
  RegisterExtension(RendererExtensionBindings::Get(this), true);
  RegisterExtension(ExtensionApiTestV8Extension::Get(), true);

  // Initialize host permissions for any extensions that were activated before
  // WebKit was initialized.
  for (std::set<std::string>::iterator iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end(); ++iter) {
    const Extension* extension = extensions_.GetByID(*iter);
    if (extension)
      InitOriginPermissions(extension);
  }

  is_webkit_initialized_ = true;
}

void ExtensionDispatcher::IdleNotification() {
  if (is_extension_process_) {
    // Dampen the forced delay as well if the extension stays idle for long
    // periods of time.
    int64 forced_delay_s = std::max(static_cast<int64>(
        RenderThread::current()->idle_notification_delay_in_s()),
        kMaxExtensionIdleHandlerDelayS);
    forced_idle_timer_.Stop();
    forced_idle_timer_.Start(FROM_HERE,
        base::TimeDelta::FromSeconds(forced_delay_s),
        RenderThread::current(), &RenderThread::IdleHandler);
  }
}

void ExtensionDispatcher::OnSetFunctionNames(
    const std::vector<std::string>& names) {
  function_names_.clear();
  for (size_t i = 0; i < names.size(); ++i)
    function_names_.insert(names[i]);
}

void ExtensionDispatcher::OnMessageInvoke(const std::string& extension_id,
                                          const std::string& function_name,
                                          const ListValue& args,
                                          const GURL& event_url) {
  ExtensionBindingsContext::DispatchChromeHiddenMethod(
      extension_id, function_name, args, NULL, event_url);

  // Reset the idle handler each time there's any activity like event or message
  // dispatch, for which Invoke is the chokepoint.
  if (is_extension_process_) {
    RenderThread::current()->ScheduleIdleHandler(
        kInitialExtensionIdleHandlerDelayS);
  }
}

void ExtensionDispatcher::OnDeliverMessage(int target_port_id,
                                           const std::string& message) {
  RendererExtensionBindings::DeliverMessage(target_port_id,
                                            message,
                                            NULL);  // All render views.
}

void ExtensionDispatcher::OnLoaded(const ExtensionMsg_Loaded_Params& params) {
  scoped_refptr<const Extension> extension(params.ConvertToExtension());
  if (!extension) {
    // This can happen if extension parsing fails for any reason. One reason
    // this can legitimately happen is if the
    // --enable-experimental-extension-apis changes at runtime, which happens
    // during browser tests. Existing renderers won't know about the change.
    return;
  }

  extensions_.Insert(extension);
}

void ExtensionDispatcher::OnUnloaded(const std::string& id) {
  extensions_.Remove(id);
  // If the extension is later reloaded with a different set of permissions,
  // we'd like it to get a new isolated world ID, so that it can pick up the
  // changed origin whitelist.
  user_script_slave_->RemoveIsolatedWorld(id);
}

void ExtensionDispatcher::OnSetScriptingWhitelist(
    const Extension::ScriptingWhitelist& extension_ids) {
  Extension::SetScriptingWhitelist(extension_ids);
}

bool ExtensionDispatcher::IsApplicationActive(
    const std::string& extension_id) const {
  return active_application_ids_.find(extension_id) !=
      active_application_ids_.end();
}

bool ExtensionDispatcher::IsExtensionActive(
    const std::string& extension_id) const {
  return active_extension_ids_.find(extension_id) !=
      active_extension_ids_.end();
}

bool ExtensionDispatcher::AllowScriptExtension(
    WebFrame* frame,
    const std::string& v8_extension_name,
    int extension_group) {
  // NULL in unit tests.
  if (!RenderThread::current())
    return true;

  // If we don't know about it, it was added by WebCore, so we should allow it.
  if (!RenderThread::current()->IsRegisteredExtension(v8_extension_name))
    return true;

  // If the V8 extension is not restricted, allow it to run anywhere.
  if (!restricted_v8_extensions_.count(v8_extension_name))
    return true;

  // Note: we prefer the provisional URL here instead of the document URL
  // because we might be currently loading an URL into a blank page.
  // See http://code.google.com/p/chromium/issues/detail?id=10924
  WebDataSource* ds = frame->provisionalDataSource();
  if (!ds)
    ds = frame->dataSource();

  // Extension-only bindings should be restricted to content scripts and
  // extension-blessed URLs.
  if (extension_group == EXTENSION_GROUP_CONTENT_SCRIPTS ||
      extensions_.ExtensionBindingsAllowed(ds->request().url())) {
    return true;
  }

  return false;

}

void ExtensionDispatcher::OnActivateApplication(
    const std::string& extension_id) {
  active_application_ids_.insert(extension_id);
}

void ExtensionDispatcher::OnActivateExtension(
    const std::string& extension_id) {
  active_extension_ids_.insert(extension_id);

  // This is called when starting a new extension page, so start the idle
  // handler ticking.
  RenderThread::current()->ScheduleIdleHandler(
      kInitialExtensionIdleHandlerDelayS);

  UpdateActiveExtensions();

  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension)
    return;

  if (is_webkit_initialized_)
    InitOriginPermissions(extension);
}

void ExtensionDispatcher::InitOriginPermissions(const Extension* extension) {
  // TODO(jstritar): We should try to remove this special case. Also, these
  // whitelist entries need to be updated when the kManagement permission
  // changes.
  if (extension->HasAPIPermission(ExtensionAPIPermission::kManagement)) {
    WebSecurityPolicy::addOriginAccessWhitelistEntry(
        extension->url(),
        WebString::fromUTF8(chrome::kChromeUIScheme),
        WebString::fromUTF8(chrome::kChromeUIExtensionIconHost),
        false);
  }

  UpdateOriginPermissions(UpdatedExtensionPermissionsInfo::ADDED,
                          extension,
                          extension->GetActivePermissions()->explicit_hosts());
}

void ExtensionDispatcher::UpdateOriginPermissions(
    UpdatedExtensionPermissionsInfo::Reason reason,
    const Extension* extension,
    const URLPatternSet& origins) {
  for (URLPatternSet::const_iterator i = origins.begin();
       i != origins.end(); ++i) {
    const char* schemes[] = {
      chrome::kHttpScheme,
      chrome::kHttpsScheme,
      chrome::kFileScheme,
      chrome::kChromeUIScheme,
    };
    for (size_t j = 0; j < arraysize(schemes); ++j) {
      if (i->MatchesScheme(schemes[j])) {
        ((reason == UpdatedExtensionPermissionsInfo::REMOVED) ?
         WebSecurityPolicy::removeOriginAccessWhitelistEntry :
         WebSecurityPolicy::addOriginAccessWhitelistEntry)(
              extension->url(),
              WebString::fromUTF8(schemes[j]),
              WebString::fromUTF8(i->host()),
              i->match_subdomains());
      }
    }
  }
}

void ExtensionDispatcher::OnUpdatePermissions(
    int reason_id,
    const std::string& extension_id,
    const ExtensionAPIPermissionSet& apis,
    const URLPatternSet& explicit_hosts,
    const URLPatternSet& scriptable_hosts) {
  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension)
    return;

  scoped_refptr<const ExtensionPermissionSet> delta =
      new ExtensionPermissionSet(apis, explicit_hosts, scriptable_hosts);
  scoped_refptr<const ExtensionPermissionSet> old_active =
      extension->GetActivePermissions();
  UpdatedExtensionPermissionsInfo::Reason reason =
      static_cast<UpdatedExtensionPermissionsInfo::Reason>(reason_id);

  const ExtensionPermissionSet* new_active = NULL;
  if (reason == UpdatedExtensionPermissionsInfo::ADDED) {
    new_active = ExtensionPermissionSet::CreateUnion(old_active, delta);
  } else {
    CHECK_EQ(UpdatedExtensionPermissionsInfo::REMOVED, reason);
    new_active = ExtensionPermissionSet::CreateDifference(old_active, delta);
  }

  extension->SetActivePermissions(new_active);
  UpdateOriginPermissions(reason, extension, explicit_hosts);
}

void ExtensionDispatcher::OnUpdateUserScripts(
    base::SharedMemoryHandle scripts) {
  DCHECK(base::SharedMemory::IsHandleValid(scripts)) << "Bad scripts handle";
  user_script_slave_->UpdateScripts(scripts);
  UpdateActiveExtensions();
}

void ExtensionDispatcher::UpdateActiveExtensions() {
  // In single-process mode, the browser process reports the active extensions.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  std::set<std::string> active_extensions = active_extension_ids_;
  user_script_slave_->GetActiveExtensions(&active_extensions);
  child_process_logging::SetActiveExtensions(active_extensions);
}

void ExtensionDispatcher::RegisterExtension(v8::Extension* extension,
                                            bool restrict_to_extensions) {
  if (restrict_to_extensions)
    restricted_v8_extensions_.insert(extension->name());

  RenderThread::current()->RegisterExtension(extension);
}
