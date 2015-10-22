// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/script_context.h"
#include "ipc/message_filter.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node.h"

namespace extensions {

namespace {

// Helper to convert an enum to a V8 object.
template <typename EnumType>
v8::Local<v8::Object> ToEnumObject(v8::Isolate* isolate,
                                   EnumType start_after,
                                   EnumType end_at) {
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  for (int i = start_after + 1; i <= end_at; ++i) {
    v8::Local<v8::String> value = v8::String::NewFromUtf8(
        isolate, ui::ToString(static_cast<EnumType>(i)).c_str());
    object->Set(value, value);
  }
  return object;
}

void ThrowInvalidArgumentsException(
    AutomationInternalCustomBindings* automation_bindings) {
  v8::Isolate* isolate = automation_bindings->GetIsolate();
  automation_bindings->GetIsolate()->ThrowException(
      v8::String::NewFromUtf8(
          isolate,
          "Invalid arguments to AutomationInternalCustomBindings function",
          v8::NewStringType::kNormal)
          .ToLocalChecked());

  LOG(FATAL) << "Invalid arguments to AutomationInternalCustomBindings function"
             << automation_bindings->context()->GetStackTraceAsString();
}

v8::Local<v8::Value> CreateV8String(v8::Isolate* isolate, const char* str) {
  return v8::String::NewFromUtf8(isolate, str, v8::String::kNormalString,
                                 strlen(str));
}

v8::Local<v8::Value> CreateV8String(v8::Isolate* isolate,
                                    const std::string& str) {
  return v8::String::NewFromUtf8(isolate, str.c_str(),
                                 v8::String::kNormalString, str.length());
}

//
// Helper class that helps implement bindings for a JavaScript function
// that takes a single input argument consisting of a Tree ID. Looks up
// the TreeCache and passes it to the function passed to the constructor.
//

typedef void (*TreeIDFunction)(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               TreeCache* cache);

class TreeIDWrapper : public base::RefCountedThreadSafe<TreeIDWrapper> {
 public:
  TreeIDWrapper(AutomationInternalCustomBindings* automation_bindings,
                TreeIDFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() != 1 || !args[0]->IsNumber())
      ThrowInvalidArgumentsException(automation_bindings_);

    int tree_id = args[0]->Int32Value();
    TreeCache* cache = automation_bindings_->GetTreeCacheFromTreeID(tree_id);
    if (!cache)
      return;

    // The root can be null if this is called from an onTreeChange callback.
    if (!cache->tree.root())
      return;

    function_(isolate, args.GetReturnValue(), cache);
  }

 private:
  virtual ~TreeIDWrapper() {}

  friend class base::RefCountedThreadSafe<TreeIDWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  TreeIDFunction function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes two input arguments: a tree ID and node ID. Looks up the
// TreeCache and the AXNode and passes them to the function passed to
// the constructor.
//

typedef void (*NodeIDFunction)(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               TreeCache* cache,
                               ui::AXNode* node);

class NodeIDWrapper : public base::RefCountedThreadSafe<NodeIDWrapper> {
 public:
  NodeIDWrapper(AutomationInternalCustomBindings* automation_bindings,
                NodeIDFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber())
      ThrowInvalidArgumentsException(automation_bindings_);

    int tree_id = args[0]->Int32Value();
    int node_id = args[1]->Int32Value();

    TreeCache* cache = automation_bindings_->GetTreeCacheFromTreeID(tree_id);
    if (!cache)
      return;

    ui::AXNode* node = cache->tree.GetFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), cache, node);
  }

 private:
  virtual ~NodeIDWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDFunction function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes three input arguments: a tree ID, node ID, and string
// argument. Looks up the TreeCache and the AXNode and passes them to the
// function passed to the constructor.
//

typedef void (*NodeIDPlusAttributeFunction)(v8::Isolate* isolate,
                                            v8::ReturnValue<v8::Value> result,
                                            ui::AXNode* node,
                                            const std::string& attribute);

class NodeIDPlusAttributeWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusAttributeWrapper> {
 public:
  NodeIDPlusAttributeWrapper(
      AutomationInternalCustomBindings* automation_bindings,
      NodeIDPlusAttributeFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 3 || !args[0]->IsNumber() || !args[1]->IsNumber() ||
        !args[2]->IsString()) {
      ThrowInvalidArgumentsException(automation_bindings_);
    }

    int tree_id = args[0]->Int32Value();
    int node_id = args[1]->Int32Value();
    std::string attribute = *v8::String::Utf8Value(args[2]);

    TreeCache* cache = automation_bindings_->GetTreeCacheFromTreeID(tree_id);
    if (!cache)
      return;

    ui::AXNode* node = cache->tree.GetFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), node, attribute);
  }

 private:
  virtual ~NodeIDPlusAttributeWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusAttributeWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusAttributeFunction function_;
};

}  // namespace

TreeCache::TreeCache() {}
TreeCache::~TreeCache() {}

class AutomationMessageFilter : public IPC::MessageFilter {
 public:
  explicit AutomationMessageFilter(AutomationInternalCustomBindings* owner)
      : owner_(owner),
        removed_(false) {
    DCHECK(owner);
    content::RenderThread::Get()->AddFilter(this);
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  void Detach() {
    owner_ = nullptr;
    Remove();
  }

  // IPC::MessageFilter
  bool OnMessageReceived(const IPC::Message& message) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &AutomationMessageFilter::OnMessageReceivedOnRenderThread,
            this, message));

    // Always return false in case there are multiple
    // AutomationInternalCustomBindings instances attached to the same thread.
    return false;
  }

  void OnFilterRemoved() override {
    removed_ = true;
  }

private:
  void OnMessageReceivedOnRenderThread(const IPC::Message& message) {
    if (owner_)
      owner_->OnMessageReceived(message);
  }

  ~AutomationMessageFilter() override {
    Remove();
  }

  void Remove() {
    if (!removed_) {
      removed_ = true;
      content::RenderThread::Get()->RemoveFilter(this);
    }
  }

  AutomationInternalCustomBindings* owner_;
  bool removed_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AutomationMessageFilter);
};

AutomationInternalCustomBindings::AutomationInternalCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context), is_active_profile_(true) {
  // It's safe to use base::Unretained(this) here because these bindings
  // will only be called on a valid AutomationInternalCustomBindings instance
  // and none of the functions have any side effects.
  #define ROUTE_FUNCTION(FN) \
  RouteFunction(#FN, \
                base::Bind(&AutomationInternalCustomBindings::FN, \
                base::Unretained(this)))
  ROUTE_FUNCTION(IsInteractPermitted);
  ROUTE_FUNCTION(GetSchemaAdditions);
  ROUTE_FUNCTION(GetRoutingID);
  ROUTE_FUNCTION(StartCachingAccessibilityTrees);
  ROUTE_FUNCTION(DestroyAccessibilityTree);
  ROUTE_FUNCTION(GetChildIDAtIndex);
  #undef ROUTE_FUNCTION

  // Bindings that take a Tree ID and return a property of the tree.

  RouteTreeIDFunction(
      "GetRootID", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      TreeCache* cache) {
        result.Set(v8::Integer::New(isolate, cache->tree.root()->id()));
      });
  RouteTreeIDFunction(
      "GetDocURL", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      TreeCache* cache) {
        result.Set(
            v8::String::NewFromUtf8(isolate, cache->tree.data().url.c_str()));
      });
  RouteTreeIDFunction(
      "GetDocTitle", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                        TreeCache* cache) {
        result.Set(
            v8::String::NewFromUtf8(isolate, cache->tree.data().title.c_str()));
      });
  RouteTreeIDFunction(
      "GetDocLoaded", [](v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result, TreeCache* cache) {
        result.Set(v8::Boolean::New(isolate, cache->tree.data().loaded));
      });
  RouteTreeIDFunction("GetDocLoadingProgress",
                      [](v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result, TreeCache* cache) {
                        result.Set(v8::Number::New(
                            isolate, cache->tree.data().loading_progress));
                      });
  RouteTreeIDFunction("GetAnchorObjectID",
                      [](v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result, TreeCache* cache) {
                        result.Set(v8::Number::New(
                            isolate, cache->tree.data().sel_anchor_object_id));
                      });
  RouteTreeIDFunction("GetAnchorOffset", [](v8::Isolate* isolate,
                                            v8::ReturnValue<v8::Value> result,
                                            TreeCache* cache) {
    result.Set(v8::Number::New(isolate, cache->tree.data().sel_anchor_offset));
  });
  RouteTreeIDFunction("GetFocusObjectID",
                      [](v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result, TreeCache* cache) {
                        result.Set(v8::Number::New(
                            isolate, cache->tree.data().sel_focus_object_id));
                      });
  RouteTreeIDFunction("GetFocusOffset", [](v8::Isolate* isolate,
                                           v8::ReturnValue<v8::Value> result,
                                           TreeCache* cache) {
    result.Set(v8::Number::New(isolate, cache->tree.data().sel_focus_offset));
  });

  // Bindings that take a Tree ID and Node ID and return a property of the node.

  RouteNodeIDFunction(
      "GetParentID", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                        TreeCache* cache, ui::AXNode* node) {
        if (node->parent())
          result.Set(v8::Integer::New(isolate, node->parent()->id()));
      });
  RouteNodeIDFunction("GetChildCount", [](v8::Isolate* isolate,
                                          v8::ReturnValue<v8::Value> result,
                                          TreeCache* cache, ui::AXNode* node) {
    result.Set(v8::Integer::New(isolate, node->child_count()));
  });
  RouteNodeIDFunction(
      "GetIndexInParent",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         TreeCache* cache, ui::AXNode* node) {
        result.Set(v8::Integer::New(isolate, node->index_in_parent()));
      });
  RouteNodeIDFunction(
      "GetState", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                     TreeCache* cache, ui::AXNode* node) {
        v8::Local<v8::Object> state(v8::Object::New(isolate));
        uint32 state_pos = 0, state_shifter = node->data().state;
        while (state_shifter) {
          if (state_shifter & 1) {
            std::string key = ToString(static_cast<ui::AXState>(state_pos));
            state->Set(CreateV8String(isolate, key),
                       v8::Boolean::New(isolate, true));
          }
          state_shifter = state_shifter >> 1;
          state_pos++;
        }
        result.Set(state);
      });
  RouteNodeIDFunction(
      "GetRole", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                    TreeCache* cache, ui::AXNode* node) {
        std::string role_name = ui::ToString(node->data().role);
        result.Set(v8::String::NewFromUtf8(isolate, role_name.c_str()));
      });
  RouteNodeIDFunction(
      "GetLocation", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                        TreeCache* cache, ui::AXNode* node) {
        v8::Local<v8::Object> location_obj(v8::Object::New(isolate));
        gfx::Rect location = node->data().location;
        location.Offset(cache->location_offset);
        location_obj->Set(CreateV8String(isolate, "left"),
                          v8::Integer::New(isolate, location.x()));
        location_obj->Set(CreateV8String(isolate, "top"),
                          v8::Integer::New(isolate, location.y()));
        location_obj->Set(CreateV8String(isolate, "width"),
                          v8::Integer::New(isolate, location.width()));
        location_obj->Set(CreateV8String(isolate, "height"),
                          v8::Integer::New(isolate, location.height()));
        result.Set(location_obj);
      });

  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusAttributeFunction(
      "GetStringAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXNode* node, const std::string& attribute_name) {
        ui::AXStringAttribute attribute =
            ui::ParseAXStringAttribute(attribute_name);
        std::string attr_value;
        if (!node->data().GetStringAttribute(attribute, &attr_value))
          return;

        result.Set(v8::String::NewFromUtf8(isolate, attr_value.c_str()));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetBoolAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXNode* node, const std::string& attribute_name) {
        ui::AXBoolAttribute attribute =
            ui::ParseAXBoolAttribute(attribute_name);
        bool attr_value;
        if (!node->data().GetBoolAttribute(attribute, &attr_value))
          return;

        result.Set(v8::Boolean::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXNode* node, const std::string& attribute_name) {
        ui::AXIntAttribute attribute = ui::ParseAXIntAttribute(attribute_name);
        int attr_value;
        if (!node->data().GetIntAttribute(attribute, &attr_value))
          return;

        result.Set(v8::Integer::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetFloatAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXNode* node, const std::string& attribute_name) {
        ui::AXFloatAttribute attribute =
            ui::ParseAXFloatAttribute(attribute_name);
        float attr_value;

        if (!node->data().GetFloatAttribute(attribute, &attr_value))
          return;

        result.Set(v8::Number::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntListAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXNode* node, const std::string& attribute_name) {
        ui::AXIntListAttribute attribute =
            ui::ParseAXIntListAttribute(attribute_name);
        if (!node->data().HasIntListAttribute(attribute))
          return;
        const std::vector<int32>& attr_value =
            node->data().GetIntListAttribute(attribute);

        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, attr_value.size()));
        for (size_t i = 0; i < attr_value.size(); ++i)
          array_result->Set(static_cast<uint32>(i),
                            v8::Integer::New(isolate, attr_value[i]));
        result.Set(array_result);
      });
  RouteNodeIDPlusAttributeFunction(
      "GetHtmlAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXNode* node, const std::string& attribute_name) {
        std::string attr_value;
        if (!node->data().GetHtmlAttribute(attribute_name.c_str(), &attr_value))
          return;

        result.Set(v8::String::NewFromUtf8(isolate, attr_value.c_str()));
      });
}

AutomationInternalCustomBindings::~AutomationInternalCustomBindings() {
  if (message_filter_)
    message_filter_->Detach();
  STLDeleteContainerPairSecondPointers(tree_id_to_tree_cache_map_.begin(),
                                       tree_id_to_tree_cache_map_.end());
}

void AutomationInternalCustomBindings::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AutomationInternalCustomBindings, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityEvent, OnAccessibilityEvent)
  IPC_END_MESSAGE_MAP()
}

TreeCache* AutomationInternalCustomBindings::GetTreeCacheFromTreeID(
    int tree_id) {
  const auto iter = tree_id_to_tree_cache_map_.find(tree_id);
  if (iter == tree_id_to_tree_cache_map_.end())
    return nullptr;

  return iter->second;
}

void AutomationInternalCustomBindings::IsInteractPermitted(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  const Extension* extension = context()->extension();
  CHECK(extension);
  const AutomationInfo* automation_info = AutomationInfo::Get(extension);
  CHECK(automation_info);
  args.GetReturnValue().Set(
      v8::Boolean::New(GetIsolate(), automation_info->interact));
}

void AutomationInternalCustomBindings::GetRoutingID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  int routing_id = context()->GetRenderFrame()->GetRenderView()->GetRoutingID();
  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), routing_id));
}

void AutomationInternalCustomBindings::StartCachingAccessibilityTrees(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!message_filter_)
    message_filter_ = new AutomationMessageFilter(this);
}

void AutomationInternalCustomBindings::GetSchemaAdditions(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::Object> additions = v8::Object::New(GetIsolate());

  additions->Set(
      v8::String::NewFromUtf8(GetIsolate(), "EventType"),
      ToEnumObject(GetIsolate(), ui::AX_EVENT_NONE, ui::AX_EVENT_LAST));

  additions->Set(
      v8::String::NewFromUtf8(GetIsolate(), "RoleType"),
      ToEnumObject(GetIsolate(), ui::AX_ROLE_NONE, ui::AX_ROLE_LAST));

  additions->Set(
      v8::String::NewFromUtf8(GetIsolate(), "StateType"),
      ToEnumObject(GetIsolate(), ui::AX_STATE_NONE, ui::AX_STATE_LAST));

  additions->Set(
      v8::String::NewFromUtf8(GetIsolate(), "TreeChangeType"),
      ToEnumObject(GetIsolate(), ui::AX_MUTATION_NONE, ui::AX_MUTATION_LAST));

  args.GetReturnValue().Set(additions);
}

void AutomationInternalCustomBindings::DestroyAccessibilityTree(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsNumber()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  int tree_id = args[0]->Int32Value();
  auto iter = tree_id_to_tree_cache_map_.find(tree_id);
  if (iter == tree_id_to_tree_cache_map_.end())
    return;

  TreeCache* cache = iter->second;
  tree_id_to_tree_cache_map_.erase(tree_id);
  axtree_to_tree_cache_map_.erase(&cache->tree);
  delete cache;
}

void AutomationInternalCustomBindings::RouteTreeIDFunction(
    const std::string& name,
    TreeIDFunction callback) {
  scoped_refptr<TreeIDWrapper> wrapper = new TreeIDWrapper(this, callback);
  RouteFunction(name, base::Bind(&TreeIDWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDFunction(
    const std::string& name,
    NodeIDFunction callback) {
  scoped_refptr<NodeIDWrapper> wrapper = new NodeIDWrapper(this, callback);
  RouteFunction(name, base::Bind(&NodeIDWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusAttributeFunction(
    const std::string& name,
    NodeIDPlusAttributeFunction callback) {
  scoped_refptr<NodeIDPlusAttributeWrapper> wrapper =
      new NodeIDPlusAttributeWrapper(this, callback);
  RouteFunction(name, base::Bind(&NodeIDPlusAttributeWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::GetChildIDAtIndex(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 3 || !args[2]->IsNumber()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  int tree_id = args[0]->Int32Value();
  int node_id = args[1]->Int32Value();

  const auto iter = tree_id_to_tree_cache_map_.find(tree_id);
  if (iter == tree_id_to_tree_cache_map_.end())
    return;

  TreeCache* cache = iter->second;
  if (!cache)
    return;

  ui::AXNode* node = cache->tree.GetFromId(node_id);
  if (!node)
    return;

  int index = args[2]->Int32Value();
  if (index < 0 || index >= node->child_count())
    return;

  int child_id = node->children()[index]->id();
  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), child_id));
}

//
// Handle accessibility events from the browser process.
//

void AutomationInternalCustomBindings::OnAccessibilityEvent(
    const ExtensionMsg_AccessibilityEventParams& params,
    bool is_active_profile) {
  is_active_profile_ = is_active_profile;
  int tree_id = params.tree_id;
  TreeCache* cache;
  auto iter = tree_id_to_tree_cache_map_.find(tree_id);
  if (iter == tree_id_to_tree_cache_map_.end()) {
    cache = new TreeCache();
    cache->tab_id = -1;
    cache->tree_id = params.tree_id;
    cache->tree.SetDelegate(this);
    tree_id_to_tree_cache_map_.insert(std::make_pair(tree_id, cache));
    axtree_to_tree_cache_map_.insert(std::make_pair(&cache->tree, cache));
  } else {
    cache = iter->second;
  }

  // Update the internal state whether it's the active profile or not.
  cache->location_offset = params.location_offset;
  if (!cache->tree.Unserialize(params.update)) {
    LOG(ERROR) << cache->tree.error();
    return;
  }

  // Don't send the event if it's not the active profile.
  if (!is_active_profile)
    return;

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> args(v8::Array::New(GetIsolate(), 1U));
  v8::Local<v8::Object> event_params(v8::Object::New(GetIsolate()));
  event_params->Set(CreateV8String(isolate, "treeID"),
                    v8::Integer::New(GetIsolate(), params.tree_id));
  event_params->Set(CreateV8String(isolate, "targetID"),
                    v8::Integer::New(GetIsolate(), params.id));
  event_params->Set(CreateV8String(isolate, "eventType"),
                    CreateV8String(isolate, ToString(params.event_type)));
  args->Set(0U, event_params);
  context()->DispatchEvent("automationInternal.onAccessibilityEvent", args);
}

void AutomationInternalCustomBindings::OnTreeDataChanged(ui::AXTree* tree) {}

void AutomationInternalCustomBindings::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                           ui::AXNode* node) {
  SendTreeChangeEvent(
      api::automation::TREE_CHANGE_TYPE_NODEREMOVED,
      tree, node);
}

void AutomationInternalCustomBindings::OnSubtreeWillBeDeleted(
    ui::AXTree* tree,
    ui::AXNode* node) {
  // This isn't strictly needed, as OnNodeWillBeDeleted will already be
  // called. We could send a JS event for this only if it turns out to
  // be needed for something.
}

void AutomationInternalCustomBindings::OnNodeCreated(ui::AXTree* tree,
                                                     ui::AXNode* node) {
  // Not needed, this is called in the middle of an update so it's not
  // safe to trigger JS from here. Wait for the notification in
  // OnAtomicUpdateFinished instead.
}

void AutomationInternalCustomBindings::OnNodeChanged(ui::AXTree* tree,
                                                     ui::AXNode* node) {
  // Not needed, this is called in the middle of an update so it's not
  // safe to trigger JS from here. Wait for the notification in
  // OnAtomicUpdateFinished instead.
}

void AutomationInternalCustomBindings::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  auto iter = axtree_to_tree_cache_map_.find(tree);
  if (iter == axtree_to_tree_cache_map_.end())
    return;

  for (auto change : changes) {
    ui::AXNode* node = change.node;
    switch (change.type) {
      case NODE_CREATED:
        SendTreeChangeEvent(
            api::automation::TREE_CHANGE_TYPE_NODECREATED,
            tree, node);
        break;
      case SUBTREE_CREATED:
        SendTreeChangeEvent(
            api::automation::TREE_CHANGE_TYPE_SUBTREECREATED,
            tree, node);
        break;
      case NODE_CHANGED:
        SendTreeChangeEvent(
            api::automation::TREE_CHANGE_TYPE_NODECHANGED,
            tree, node);
        break;
    }
  }
}

void AutomationInternalCustomBindings::SendTreeChangeEvent(
    api::automation::TreeChangeType change_type,
    ui::AXTree* tree,
    ui::AXNode* node) {
  // Don't send tree change events when it's not the active profile.
  if (!is_active_profile_)
    return;

  auto iter = axtree_to_tree_cache_map_.find(tree);
  if (iter == axtree_to_tree_cache_map_.end())
    return;

  int tree_id = iter->second->tree_id;

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> args(v8::Array::New(GetIsolate(), 3U));
  args->Set(0U, v8::Integer::New(GetIsolate(), tree_id));
  args->Set(1U, v8::Integer::New(GetIsolate(), node->id()));
  args->Set(2U, CreateV8String(isolate, ToString(change_type)));
  context()->DispatchEvent("automationInternal.onTreeChange", args);
}

}  // namespace extensions
