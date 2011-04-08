// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_dispatcher.h"

#include "base/command_line.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extension_groups.h"
#include "chrome/renderer/extensions/chrome_app_bindings.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/js_only_v8_extensions.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "chrome/renderer/render_thread.h"
#include "v8/include/v8.h"

namespace {
static const double kInitialExtensionIdleHandlerDelayS = 5.0 /* seconds */;
static const int64 kMaxExtensionIdleHandlerDelayS = 5*60 /* seconds */;
static ExtensionDispatcher* g_extension_dispatcher;
}

using WebKit::WebFrame;

ExtensionDispatcher* ExtensionDispatcher::Get() {
  return g_extension_dispatcher;
}

ExtensionDispatcher::ExtensionDispatcher() {
  DCHECK(!g_extension_dispatcher);
  g_extension_dispatcher = this;

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
  g_extension_dispatcher = NULL;
}

bool ExtensionDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionDispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetFunctionNames, OnSetFunctionNames)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Loaded, OnLoaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Unloaded, OnUnloaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetScriptingWhitelist,
                        OnSetScriptingWhitelist)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdatePageActions, OnPageActionsUpdated)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetAPIPermissions, OnSetAPIPermissions)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetHostPermissions, OnSetHostPermissions)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateUserScripts, OnUpdateUserScripts)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ExtensionDispatcher::OnRenderProcessShutdown() {
  delete this;
}

void ExtensionDispatcher::WebKitInitialized() {
  // For extensions, we want to ensure we call the IdleHandler every so often,
  // even if the extension keeps up activity.
  if (is_extension_process_) {
    forced_idle_timer_.Start(
        base::TimeDelta::FromSeconds(kMaxExtensionIdleHandlerDelayS),
        RenderThread::current(), &RenderThread::IdleHandler);
  }

  RegisterExtension(extensions_v8::ChromeAppExtension::Get(), false);

  // Add v8 extensions related to chrome extensions.
  RegisterExtension(ExtensionProcessBindings::Get(), true);
  RegisterExtension(BaseJsV8Extension::Get(), true);
  RegisterExtension(JsonSchemaJsV8Extension::Get(), true);
  RegisterExtension(EventBindings::Get(), true);
  RegisterExtension(RendererExtensionBindings::Get(), true);
  RegisterExtension(ExtensionApiTestV8Extension::Get(), true);
}

bool ExtensionDispatcher::AllowScriptExtension(
    const std::string& v8_extension_name,
    const GURL& url,
    int extension_group) {
  // If the V8 extension is not restricted, allow it to run anywhere.
  if (!restricted_v8_extensions_.count(v8_extension_name))
    return true;

  // Extension-only bindings should be restricted to content scripts and
  // extension-blessed URLs.
  if (extension_group == EXTENSION_GROUP_CONTENT_SCRIPTS ||
      extensions_.ExtensionBindingsAllowed(url)) {
    return true;
  }

  return false;
}

void ExtensionDispatcher::IdleNotification() {
  if (is_extension_process_) {
    // Dampen the forced delay as well if the extension stays idle for long
    // periods of time.
    int64 forced_delay_s = std::max(static_cast<int64>(
        RenderThread::current()->idle_notification_delay_in_s()),
        kMaxExtensionIdleHandlerDelayS);
    forced_idle_timer_.Stop();
    forced_idle_timer_.Start(
        base::TimeDelta::FromSeconds(forced_delay_s),
        RenderThread::current(), &RenderThread::IdleHandler);
  }
}

void ExtensionDispatcher::OnSetFunctionNames(
    const std::vector<std::string>& names) {
  ExtensionProcessBindings::SetFunctionNames(names);
}

void ExtensionDispatcher::OnMessageInvoke(const std::string& extension_id,
                                          const std::string& function_name,
                                          const ListValue& args,
                                          const GURL& event_url) {
  RendererExtensionBindings::Invoke(
      extension_id, function_name, args, NULL, event_url);

  // Reset the idle handler each time there's any activity like event or message
  // dispatch, for which Invoke is the chokepoint.
  if (is_extension_process_) {
    RenderThread::current()->ScheduleIdleHandler(
        kInitialExtensionIdleHandlerDelayS);
  }
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
}

void ExtensionDispatcher::OnSetScriptingWhitelist(
    const Extension::ScriptingWhitelist& extension_ids) {
  Extension::SetScriptingWhitelist(extension_ids);
}

void ExtensionDispatcher::OnPageActionsUpdated(
    const std::string& extension_id,
    const std::vector<std::string>& page_actions) {
  ExtensionProcessBindings::SetPageActions(extension_id, page_actions);
}

void ExtensionDispatcher::OnSetAPIPermissions(
    const std::string& extension_id,
    const std::set<std::string>& permissions) {
  ExtensionProcessBindings::SetAPIPermissions(extension_id, permissions);

  // This is called when starting a new extension page, so start the idle
  // handler ticking.
  RenderThread::current()->ScheduleIdleHandler(
      kInitialExtensionIdleHandlerDelayS);

  UpdateActiveExtensions();
}

void ExtensionDispatcher::OnSetHostPermissions(
    const GURL& extension_url, const std::vector<URLPattern>& permissions) {
  ExtensionProcessBindings::SetHostPermissions(extension_url, permissions);
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

  std::set<std::string> active_extensions;
  user_script_slave_->GetActiveExtensions(&active_extensions);
  ExtensionProcessBindings::GetActiveExtensions(&active_extensions);
  child_process_logging::SetActiveExtensions(active_extensions);
}

void ExtensionDispatcher::RegisterExtension(v8::Extension* extension,
                                            bool restrict_to_extensions) {
  if (restrict_to_extensions)
    restricted_v8_extensions_.insert(extension->name());

  RenderThread::current()->RegisterExtension(extension);
}
