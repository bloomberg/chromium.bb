// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/common/extensions/api/automation.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_tree.h"
#include "v8/include/v8.h"

struct ExtensionMsg_AccessibilityEventParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace extensions {

class AutomationInternalCustomBindings;
class AutomationMessageFilter;

struct TreeCache {
  TreeCache();
  ~TreeCache();

  int tab_id;
  int tree_id;
  int parent_node_id_from_parent_tree;

  gfx::Vector2d location_offset;
  ui::AXTree tree;
  AutomationInternalCustomBindings* owner;
};

struct TreeChangeObserver {
  int id;
  api::automation::TreeChangeObserverFilter filter;
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

  ui::AXNode* GetParent(ui::AXNode* node, TreeCache** in_out_cache);

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

  void AddTreeChangeObserver(const v8::FunctionCallbackInfo<v8::Value>& args);

  void RemoveTreeChangeObserver(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void GetFocus(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Given an initial TreeCache, return the TreeCache and node of the focused
  // node within this tree or a focused descendant tree.
  bool GetFocusInternal(TreeCache* top_cache,
                        TreeCache** out_cache,
                        ui::AXNode** out_node);

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
  void RouteNodeIDPlusRangeFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       TreeCache* cache,
                       ui::AXNode* node,
                       int start,
                       int end));

  //
  // Access the cached accessibility trees and properties of their nodes.
  //

  // Args: int ax_tree_id, int node_id, Returns: int child_id.
  void GetChildIDAtIndex(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id
  // Returns: JS object with a map from html attribute key to value.
  void GetHtmlAttributes(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id
  // Returns: JS object with a string key for each state flag that's set.
  void GetState(const v8::FunctionCallbackInfo<v8::Value>& args);

  //
  // Helper functions.
  //

  // Handle accessibility events from the browser process.
  void OnAccessibilityEvent(const ExtensionMsg_AccessibilityEventParams& params,
                            bool is_active_profile);
  void OnAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params);

  void UpdateOverallTreeChangeObserverFilter();

  // AXTreeDelegate implementation.
  void OnNodeDataWillChange(ui::AXTree* tree,
                            const ui::AXNodeData& old_node_data,
                            const ui::AXNodeData& new_node_data) override;
  void OnTreeDataChanged(ui::AXTree* tree) override;
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeWillBeReparented(ui::AXTree* tree, ui::AXNode* node) override;
  void OnSubtreeWillBeReparented(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeReparented(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeChanged(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(ui::AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;
  void SendTreeChangeEvent(api::automation::TreeChangeType change_type,
                           ui::AXTree* tree,
                           ui::AXNode* node);
  void SendChildTreeIDEvent(ui::AXTree* tree, ui::AXNode* node);
  void SendNodesRemovedEvent(ui::AXTree* tree, const std::vector<int>& ids);

  base::hash_map<int, TreeCache*> tree_id_to_tree_cache_map_;
  base::hash_map<ui::AXTree*, TreeCache*> axtree_to_tree_cache_map_;
  scoped_refptr<AutomationMessageFilter> message_filter_;
  bool is_active_profile_;
  std::vector<TreeChangeObserver> tree_change_observers_;
  // A bit-map of api::automation::TreeChangeObserverFilter.
  int tree_change_observer_overall_filter_;
  std::vector<int> deleted_node_ids_;
  std::vector<int> text_changed_node_ids_;

  DISALLOW_COPY_AND_ASSIGN(AutomationInternalCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
