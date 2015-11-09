// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_

#include "base/compiler_specific.h"
#include "chrome/common/extensions/api/automation.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_tree.h"
#include "v8/include/v8.h"

struct ExtensionMsg_AccessibilityEventParams;

namespace extensions {

class AutomationMessageFilter;

struct TreeCache {
  TreeCache();
  ~TreeCache();

  int tab_id;
  int tree_id;

  gfx::Vector2d location_offset;
  ui::AXTree tree;
};

// The native component of custom bindings for the chrome.automationInternal
// API.
class AutomationInternalCustomBindings : public ObjectBackedNativeHandler,
                                         public ui::AXTreeDelegate {
 public:
  explicit AutomationInternalCustomBindings(ScriptContext* context);

  ~AutomationInternalCustomBindings() override;

  void OnMessageReceived(const IPC::Message& message);

  TreeCache* GetTreeCacheFromTreeID(int tree_id);

  ScriptContext* context() const {
    return ObjectBackedNativeHandler::context();
  }

 private:
  // ObjectBackedNativeHandler overrides:
  void Invalidate() override;

  // Returns whether this extension has the "interact" permission set (either
  // explicitly or implicitly after manifest parsing).
  void IsInteractPermitted(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns an object with bindings that will be added to the
  // chrome.automation namespace.
  void GetSchemaAdditions(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Get the routing ID for the extension.
  void GetRoutingID(const v8::FunctionCallbackInfo<v8::Value>& args);

  // This is called by automation_internal_custom_bindings.js to indicate
  // that an API was called that needs access to accessibility trees. This
  // enables the MessageFilter that allows us to listen to accessibility
  // events forwarded to this process.
  void StartCachingAccessibilityTrees(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Called when an accessibility tree is destroyed and needs to be
  // removed from our cache.
  // Args: int ax_tree_id
  void DestroyAccessibilityTree(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void RouteTreeIDFunction(const std::string& name,
                           void (*callback)(v8::Isolate* isolate,
                                            v8::ReturnValue<v8::Value> result,
                                            TreeCache* cache));

  void RouteNodeIDFunction(const std::string& name,
                           void (*callback)(v8::Isolate* isolate,
                                            v8::ReturnValue<v8::Value> result,
                                            TreeCache* cache,
                                            ui::AXNode* node));

  void RouteNodeIDPlusAttributeFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       ui::AXNode* node,
                       const std::string& attribute_name));

  //
  // Access the cached accessibility trees and properties of their nodes.
  //

  // Args: int ax_tree_id, int node_id, Returns: int parent_node_id.
  void GetParentID(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, Returns: int child_count.
  void GetChildCount(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, Returns: int child_id.
  void GetChildIDAtIndex(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, Returns: int index_in_parent.
  void GetIndexInParent(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id
  // Returns: JS object with a string key for each state flag that's set.
  void GetState(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, Returns: string role_name
  void GetRole(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id
  // Returns: JS object with {left, top, width, height}
  void GetLocation(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, string attribute_name
  // Returns: string attribute_value.
  void GetStringAttribute(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, string attribute_name
  // Returns: bool attribute_value.
  void GetBoolAttribute(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, string attribute_name
  // Returns: int attribute_value.
  void GetIntAttribute(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, string attribute_name
  // Returns: float attribute_value.
  void GetFloatAttribute(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, string attribute_name
  // Returns: JS array of int attribute_values.
  void GetIntListAttribute(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id, string attribute_name
  // Returns: string attribute_value.
  void GetHtmlAttribute(const v8::FunctionCallbackInfo<v8::Value>& args);

  //
  // Helper functions.
  //

  // Handle accessibility events from the browser process.
  void OnAccessibilityEvent(const ExtensionMsg_AccessibilityEventParams& params,
                            bool is_active_profile);

  // AXTreeDelegate implementation.
  void OnTreeDataChanged(ui::AXTree* tree) override;
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeChanged(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(ui::AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;

  void SendTreeChangeEvent(api::automation::TreeChangeType change_type,
                           ui::AXTree* tree,
                           ui::AXNode* node);

  base::hash_map<int, TreeCache*> tree_id_to_tree_cache_map_;
  base::hash_map<ui::AXTree*, TreeCache*> axtree_to_tree_cache_map_;
  scoped_refptr<AutomationMessageFilter> message_filter_;
  bool is_active_profile_;

  DISALLOW_COPY_AND_ASSIGN(AutomationInternalCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
