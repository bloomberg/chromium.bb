// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_DISPATCHER_H_
#define CHROME_RENDERER_EXTENSIONS_DISPATCHER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "base/timer/timer.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/v8_schema_registry.h"
#include "chrome/renderer/resource_bundle_source_map.h"
#include "content/public/renderer/render_process_observer.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "v8/include/v8.h"

class ChromeRenderViewTest;
class GURL;
class ModuleSystem;
class URLPattern;
struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_Loaded_Params;
struct ExtensionMsg_UpdatePermissions_Params;

namespace blink {
class WebFrame;
class WebSecurityOrigin;
}

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {
class RenderThread;
}

namespace extensions {
class ContentWatcher;
class Extension;
class FilteredEventRouter;
class ManifestPermissionSet;
class RequestSender;
class ScriptContext;
class UserScriptSlave;
struct Message;

// Dispatches extension control messages sent to the renderer and stores
// renderer extension related state.
class Dispatcher : public content::RenderProcessObserver {
 public:
  Dispatcher();
  virtual ~Dispatcher();

  const std::set<std::string>& function_names() const {
    return function_names_;
  }

  bool is_extension_process() const { return is_extension_process_; }
  const ExtensionSet* extensions() const { return &extensions_; }
  const ChromeV8ContextSet& v8_context_set() const {
    return v8_context_set_;
  }
  UserScriptSlave* user_script_slave() {
    return user_script_slave_.get();
  }
  V8SchemaRegistry* v8_schema_registry() {
    return v8_schema_registry_.get();
  }
  ContentWatcher* content_watcher() {
    return content_watcher_.get();
  }
  RequestSender* request_sender() {
    return request_sender_.get();
  }

  bool IsExtensionActive(const std::string& extension_id) const;

  // Finds the extension ID for the JavaScript context associated with the
  // specified |frame| and isolated world. If |world_id| is zero, finds the
  // extension ID associated with the main world's JavaScript context. If the
  // JavaScript context isn't from an extension, returns empty string.
  std::string GetExtensionID(const blink::WebFrame* frame, int world_id);

  void DidCreateScriptContext(blink::WebFrame* frame,
                              v8::Handle<v8::Context> context,
                              int extension_group,
                              int world_id);
  void WillReleaseScriptContext(blink::WebFrame* frame,
                                v8::Handle<v8::Context> context,
                                int world_id);

  void DidCreateDocumentElement(blink::WebFrame* frame);

  void DidMatchCSS(
      blink::WebFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors);

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
  bool CheckContextAccessToExtensionAPI(const std::string& function_name,
                                        ScriptContext* context) const;

  // Dispatches the event named |event_name| to all render views.
  void DispatchEvent(const std::string& extension_id,
                     const std::string& event_name) const;

  // Shared implementation of the various MessageInvoke IPCs.
  void InvokeModuleSystemMethod(
      content::RenderView* render_view,
      const std::string& extension_id,
      const std::string& module_name,
      const std::string& function_name,
      const base::ListValue& args,
      bool user_gesture);

  void ClearPortData(int port_id);

 private:
  friend class ::ChromeRenderViewTest;
  FRIEND_TEST_ALL_PREFIXES(RendererPermissionsPolicyDelegateTest,
                           CannotScriptWebstore);
  typedef void (*BindingInstaller)(ModuleSystem* module_system,
                                  v8::Handle<v8::Object> chrome);

  // RenderProcessObserver implementation:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebKitInitialized() OVERRIDE;
  virtual void IdleNotification() OVERRIDE;
  virtual void OnRenderProcessShutdown() OVERRIDE;

  void OnSetChannel(int channel);
  void OnMessageInvoke(const std::string& extension_id,
                       const std::string& module_name,
                       const std::string& function_name,
                       const base::ListValue& args,
                       bool user_gesture);
  void OnDispatchOnConnect(int target_port_id,
                           const std::string& channel_name,
                           const base::DictionaryValue& source_tab,
                           const ExtensionMsg_ExternalConnectionInfo& info,
                           const std::string& tls_channel_id);
  void OnDeliverMessage(int target_port_id, const Message& message);
  void OnDispatchOnDisconnect(int port_id, const std::string& error_message);
  void OnSetFunctionNames(const std::vector<std::string>& names);
  void OnSetSystemFont(const std::string& font_family,
                       const std::string& font_size);
  void OnLoaded(
      const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions);
  void OnLoadedInternal(scoped_refptr<const Extension> extension);
  void OnUnloaded(const std::string& id);
  void OnSetScriptingWhitelist(
      const ExtensionsClient::ScriptingWhitelist& extension_ids);
  void OnPageActionsUpdated(const std::string& extension_id,
      const std::vector<std::string>& page_actions);
  void OnActivateExtension(const std::string& extension_id);
  void OnUpdatePermissions(const ExtensionMsg_UpdatePermissions_Params& params);
  void OnUpdateTabSpecificPermissions(int page_id,
                                      int tab_id,
                                      const std::string& extension_id,
                                      const URLPatternSet& origin_set);
  void OnClearTabSpecificPermissions(
      int tab_id,
      const std::vector<std::string>& extension_ids);
  void OnUpdateUserScripts(base::SharedMemoryHandle table);
  void OnUsingWebRequestAPI(
      bool adblock,
      bool adblock_plus,
      bool other_webrequest);
  void OnShouldSuspend(const std::string& extension_id, int sequence_id);
  void OnSuspend(const std::string& extension_id);
  void OnCancelSuspend(const std::string& extension_id);

  // Update the list of active extensions that will be reported when we crash.
  void UpdateActiveExtensions();

  // Sets up the host permissions for |extension|.
  void InitOriginPermissions(const Extension* extension,
                             Feature::Context context_type);
  void AddOrRemoveOriginPermissions(
      UpdatedExtensionPermissionsInfo::Reason reason,
      const Extension* extension,
      const URLPatternSet& origins);

  // Enable custom element whitelist in Apps.
  void EnableCustomElementWhiteList();

  // Adds or removes bindings for every context belonging to |extension_id|, or
  // or all contexts if |extension_id| is empty.
  void AddOrRemoveBindings(const std::string& extension_id);

  void RegisterNativeHandlers(ModuleSystem* module_system,
                              ChromeV8Context* context);
  void AddOrRemoveBindingsForContext(ChromeV8Context* context);
  void RegisterBinding(const std::string& api_name,
                       ChromeV8Context* context);
  v8::Handle<v8::Object> GetOrCreateBindObjectIfAvailable(
      const std::string& api_name,
      std::string* bind_name,
      ChromeV8Context* context);

  // Inserts static source code into |source_map_|.
  void PopulateSourceMap();

  // Inserts BindingInstallers into |lazy_bindings_map_|.
  void PopulateLazyBindingsMap();

  // Sets up the bindings for the given api.
  void InstallBindings(ModuleSystem* module_system,
                       v8::Handle<v8::Context> v8_context,
                       const std::string& api);

  // Returns whether the current renderer hosts a platform app.
  bool IsWithinPlatformApp();

  bool IsSandboxedPage(const GURL& url) const;

  // Returns the Feature::Context type of context for a JavaScript context.
  Feature::Context ClassifyJavaScriptContext(
      const Extension* extension,
      int extension_group,
      const GURL& url,
      const blink::WebSecurityOrigin& origin);

  // Gets |field| from |object| or creates it as an empty object if it doesn't
  // exist.
  v8::Handle<v8::Object> GetOrCreateObject(v8::Handle<v8::Object> object,
                                           const std::string& field,
                                           v8::Isolate* isolate);

  // True if this renderer is running extensions.
  bool is_extension_process_;

  // Contains all loaded extensions.  This is essentially the renderer
  // counterpart to ExtensionService in the browser. It contains information
  // about all extensions currently loaded by the browser.
  ExtensionSet extensions_;

  // The IDs of extensions that failed to load, mapped to the error message
  // generated on failure.
  std::map<std::string, std::string> extension_load_errors_;

  // All the bindings contexts that are currently loaded for this renderer.
  // There is zero or one for each v8 context.
  ChromeV8ContextSet v8_context_set_;

  scoped_ptr<UserScriptSlave> user_script_slave_;

  scoped_ptr<ContentWatcher> content_watcher_;

  // Same as above, but on a longer timer and will run even if the process is
  // not idle, to ensure that IdleHandle gets called eventually.
  scoped_ptr<base::RepeatingTimer<content::RenderThread> > forced_idle_timer_;

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
  scoped_ptr<V8SchemaRegistry> v8_schema_registry_;

  // Bindings that are defined lazily and have BindingInstallers to install
  // them.
  std::map<std::string, BindingInstaller> lazy_bindings_map_;

  // Sends API requests to the extension host.
  scoped_ptr<RequestSender> request_sender_;

  // The platforms system font family and size;
  std::string system_font_family_;
  std::string system_font_size_;

  // Mapping of port IDs to tabs. If there is no tab, the value would be -1.
  std::map<int, int> port_to_tab_id_map_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_DISPATCHER_H_
