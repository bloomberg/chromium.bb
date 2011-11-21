// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "v8/include/v8.h"

class GURL;
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

  bool IsApplicationActive(const std::string& extension_id) const;
  bool IsExtensionActive(const std::string& extension_id) const;

  // See WebKit::WebPermissionClient::allowScriptExtension
  bool AllowScriptExtension(WebKit::WebFrame* frame,
                            const std::string& v8_extension_name,
                            int extension_group);

  void DidCreateScriptContext(WebKit::WebFrame* frame,
                              v8::Handle<v8::Context> context,
                              int world_id);
  void WillReleaseScriptContext(WebKit::WebFrame* frame,
                                v8::Handle<v8::Context> context,
                                int world_id);

  void SetTestExtensionId(const std::string& extension_id);

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

  // If the extension is in fact idle, tell the browser process to close
  // the background page.
  void CheckIdleStatus(const std::string& extension_id);

 private:
  friend class RenderViewTest;

  // RenderProcessObserver implementation:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebKitInitialized() OVERRIDE;
  virtual void IdleNotification() OVERRIDE;

  void OnMessageInvoke(const std::string& extension_id,
                       const std::string& function_name,
                       const base::ListValue& args,
                       const GURL& event_url);
  void OnDeliverMessage(int target_port_id, const std::string& message);
  void OnSetFunctionNames(const std::vector<std::string>& names);
  void OnLoaded(
      const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions);
  void OnUnloaded(const std::string& id);
  void OnSetScriptingWhitelist(
      const Extension::ScriptingWhitelist& extension_ids);
  void OnPageActionsUpdated(const std::string& extension_id,
      const std::vector<std::string>& page_actions);
  void OnActivateApplication(const std::string& extension_id);
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

  // All declared function names from extension_api.json.
  std::set<std::string> function_names_;

  // The extensions that are active in this process.
  std::set<std::string> active_extension_ids_;

  // The applications that are active in this process.
  std::set<std::string> active_application_ids_;

  // True once WebKit has been initialized (and it is therefore safe to poke).
  bool is_webkit_initialized_;

  std::string test_extension_id_;

  // Status of webrequest usage for known extensions.
  // TODO(mpcomplete): remove. http://crbug.com/100411
  bool webrequest_adblock_;
  bool webrequest_adblock_plus_;
  bool webrequest_other_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDispatcher);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_DISPATCHER_H_
