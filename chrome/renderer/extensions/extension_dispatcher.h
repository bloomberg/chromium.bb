// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_DISPATCHER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_DISPATCHER_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/shared_memory.h"
#include "base/timer.h"
#include "content/public/renderer/render_process_observer.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/feature.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/v8_schema_registry.h"
#include "chrome/renderer/resource_bundle_source_map.h"
#include "v8/include/v8.h"

class ExtensionRequestSender;
class GURL;
class ModuleSystem;
class URLPattern;
class UserScriptSlave;
struct ExtensionMsg_Loaded_Params;

namespace WebKit {
class WebFrame;
}

namespace base {
class ListValue;
}

namespace content {
class RenderThread;
}

// Dispatches extension control messages sent to the renderer and stores
// renderer extension related state.
class ExtensionDispatcher : public content::RenderProcessObserver {
 public:
  ExtensionDispatcher();
  virtual ~ExtensionDispatcher();

  const std::set<std::string>& function_names() const {
    return function_names_;
  }

  bool is_extension_process() const { return is_extension_process_; }
  const ExtensionSet* extensions() const { return &extensions_; }
  const ChromeV8ContextSet& v8_context_set() const {
    return v8_context_set_;
  }
  UserScriptSlave* user_script_slave() { return user_script_slave_.get(); }
  extensions::V8SchemaRegistry* v8_schema_registry() {
    return &v8_schema_registry_;
  }

  bool IsExtensionActive(const std::string& extension_id) const;

  // Finds the extension ID for the JavaScript context associated with the
  // specified |frame| and isolated world. If |world_id| is zero, finds the
  // extension ID associated with the main world's JavaScript context. If the
  // JavaScript context isn't from an extension, returns empty string.
  std::string GetExtensionID(WebKit::WebFrame* frame, int world_id);

  // See WebKit::WebPermissionClient::allowScriptExtension
  // TODO(koz): Remove once WebKit no longer calls this.
  bool AllowScriptExtension(WebKit::WebFrame* frame,
                            const std::string& v8_extension_name,
                            int extension_group);

  // TODO(koz): Remove once WebKit no longer calls this.
  bool AllowScriptExtension(WebKit::WebFrame* frame,
                            const std::string& v8_extension_name,
                            int extension_group,
                            int world_id);

  void DidCreateScriptContext(WebKit::WebFrame* frame,
                              v8::Handle<v8::Context> context,
                              int extension_group,
                              int world_id);
  void WillReleaseScriptContext(WebKit::WebFrame* frame,
                                v8::Handle<v8::Context> context,
                                int world_id);

  // TODO(mpcomplete): remove. http://crbug.com/100411
  bool IsAdblockWithWebRequestInstalled() const {
    return webrequest_adblock_;
  }
  bool IsAdblockPlusWithWebRequestInstalled() const {
    return webrequest_adblock_plus_;
  }
  bool IsOtherExtensionWithWebRequestInstalled() const {
    return webrequest_other_;
  }

  void OnExtensionResponse(int request_id,
                           bool success,
                           const base::ListValue& response,
                           const std::string& error);

  // Checks that the current context contains an extension that has permission
  // to execute the specified function. If it does not, a v8 exception is thrown
  // and the method returns false. Otherwise returns true.
  bool CheckCurrentContextAccessToExtensionAPI(
      const std::string& function_name) const;

 private:
  friend class RenderViewTest;
  typedef void (*BindingInstaller)(ModuleSystem* module_system,
                                  v8::Handle<v8::Object> chrome,
                                  v8::Handle<v8::Object> chrome_hidden);

  // RenderProcessObserver implementation:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebKitInitialized() OVERRIDE;
  virtual void IdleNotification() OVERRIDE;

  void OnSetChannel(int channel);
  void OnMessageInvoke(const std::string& extension_id,
                       const std::string& function_name,
                       const base::ListValue& args,
                       const GURL& event_url,
                       bool user_gesture);
  void OnDispatchOnConnect(int target_port_id,
                           const std::string& channel_name,
                           const std::string& tab_json,
                           const std::string& source_extension_id,
                           const std::string& target_extension_id);
  void OnDeliverMessage(int target_port_id, const std::string& message);
  void OnDispatchOnDisconnect(int port_id, bool connection_error);
  void OnSetFunctionNames(const std::vector<std::string>& names);
  void OnLoaded(
      const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions);
  void OnUnloaded(const std::string& id);
  void OnSetScriptingWhitelist(
      const Extension::ScriptingWhitelist& extension_ids);
  void OnPageActionsUpdated(const std::string& extension_id,
      const std::vector<std::string>& page_actions);
  void OnActivateExtension(const std::string& extension_id);
  void OnUpdatePermissions(int reason_id,
                           const std::string& extension_id,
                           const ExtensionAPIPermissionSet& apis,
                           const URLPatternSet& explicit_hosts,
                           const URLPatternSet& scriptable_hosts);
  void OnUpdateUserScripts(base::SharedMemoryHandle table);
  void OnUsingWebRequestAPI(
      bool adblock,
      bool adblock_plus,
      bool other_webrequest);
  void OnShouldUnload(const std::string& extension_id, int sequence_id);
  void OnUnload(const std::string& extension_id);

  // Update the list of active extensions that will be reported when we crash.
  void UpdateActiveExtensions();

  // Calls RenderThread's RegisterExtension and keeps tracks of which v8
  // extension is for Chrome Extensions only.
  void RegisterExtension(v8::Extension* extension, bool restrict_to_extensions);

  // Sets up the host permissions for |extension|.
  void InitOriginPermissions(const Extension* extension);
  void UpdateOriginPermissions(UpdatedExtensionPermissionsInfo::Reason reason,
                               const Extension* extension,
                               const URLPatternSet& origins);

  void RegisterNativeHandlers(ModuleSystem* module_system,
                              ChromeV8Context* context);

  // Inserts static source code into |source_map_|.
  void PopulateSourceMap();

  // Inserts BindingInstallers into |lazy_bindings_map_|.
  void PopulateLazyBindingsMap();

  // Sets up the bindings for the given api.
  void InstallBindings(ModuleSystem* module_system,
                       v8::Handle<v8::Context> v8_context,
                       const std::string& api);

  // Returns the Feature::Context type of context for a JavaScript context.
  extensions::Feature::Context ClassifyJavaScriptContext(
      const std::string& extension_id,
      int extension_group,
      const ExtensionURLInfo& url_info);

  // True if this renderer is running extensions.
  bool is_extension_process_;

  // Contains all loaded extensions.  This is essentially the renderer
  // counterpart to ExtensionService in the browser. It contains information
  // about all extensions currently loaded by the browser.
  ExtensionSet extensions_;

  // All the bindings contexts that are currently loaded for this renderer.
  // There is zero or one for each v8 context.
  ChromeV8ContextSet v8_context_set_;

  scoped_ptr<UserScriptSlave> user_script_slave_;

  // Same as above, but on a longer timer and will run even if the process is
  // not idle, to ensure that IdleHandle gets called eventually.
  base::RepeatingTimer<content::RenderThread> forced_idle_timer_;

  // The v8 extensions which are restricted to extension-related contexts.
  std::set<std::string> restricted_v8_extensions_;

  // All declared function names.
  std::set<std::string> function_names_;

  // The extensions and apps that are active in this process.
  std::set<std::string> active_extension_ids_;

  // True once WebKit has been initialized (and it is therefore safe to poke).
  bool is_webkit_initialized_;

  // Status of webrequest usage for known extensions.
  // TODO(mpcomplete): remove. http://crbug.com/100411
  bool webrequest_adblock_;
  bool webrequest_adblock_plus_;
  bool webrequest_other_;

  ResourceBundleSourceMap source_map_;

  // Cache for the v8 representation of extension API schemas.
  extensions::V8SchemaRegistry v8_schema_registry_;

  // Bindings that are defined lazily and have BindingInstallers to install
  // them.
  std::map<std::string, BindingInstaller> lazy_bindings_map_;

  // Sends API requests to the extension host.
  scoped_ptr<ExtensionRequestSender> request_sender_;

  // The current channel. From VersionInfo::GetChannel().
  // TODO(aa): Remove when we can restrict non-permission APIs to dev-only.
  int chrome_channel_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDispatcher);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_DISPATCHER_H_
