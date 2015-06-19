// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/script_context.h"
#include "ipc/message_filter.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node.h"

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

}  // namespace

namespace extensions {

TreeCache::TreeCache() {}
TreeCache::~TreeCache() {}

class AutomationMessageFilter : public IPC::MessageFilter {
 public:
  explicit AutomationMessageFilter(AutomationInternalCustomBindings* owner)
      : owner_(owner),
        removed_(false) {
    DCHECK(owner);
    content::RenderThread::Get()->AddFilter(this);
    task_runner_ = content::RenderThread::Get()->GetTaskRunner();
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
    ScriptContext* context) : ObjectBackedNativeHandler(context) {
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
  ROUTE_FUNCTION(GetRootID);
  ROUTE_FUNCTION(GetParentID);
  ROUTE_FUNCTION(GetChildCount);
  ROUTE_FUNCTION(GetChildIDAtIndex);
  ROUTE_FUNCTION(GetIndexInParent);
  ROUTE_FUNCTION(GetState);
  ROUTE_FUNCTION(GetRole);
  ROUTE_FUNCTION(GetLocation);
  ROUTE_FUNCTION(GetStringAttribute);
  ROUTE_FUNCTION(GetBoolAttribute);
  ROUTE_FUNCTION(GetIntAttribute);
  ROUTE_FUNCTION(GetFloatAttribute);
  ROUTE_FUNCTION(GetIntListAttribute);
  ROUTE_FUNCTION(GetHtmlAttribute);

  #undef ROUTE_FUNCTION
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
  int routing_id = context()->GetRenderView()->GetRoutingID();
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
    ThrowInvalidArgumentsException(args);
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

//
// Access the cached accessibility trees and properties of their nodes.
//

void AutomationInternalCustomBindings::GetRootID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsNumber()) {
    ThrowInvalidArgumentsException(args);
    return;
  }

  int tree_id = args[0]->Int32Value();
  const auto iter = tree_id_to_tree_cache_map_.find(tree_id);
  if (iter == tree_id_to_tree_cache_map_.end())
    return;

  TreeCache* cache = iter->second;
  int root_id = cache->tree.root()->id();
  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), root_id));
}

void AutomationInternalCustomBindings::GetParentID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  if (!GetNodeHelper(args, nullptr, &node))
    return;

  if (!node->parent())
    return;

  int parent_id = node->parent()->id();
  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), parent_id));
}

void AutomationInternalCustomBindings::GetChildCount(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  if (!GetNodeHelper(args, nullptr, &node))
    return;

  int child_count = node->child_count();
  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), child_count));
}

void AutomationInternalCustomBindings::GetChildIDAtIndex(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 3 || !args[2]->IsNumber()) {
    ThrowInvalidArgumentsException(args);
    return;
  }

  ui::AXNode* node = nullptr;
  if (!GetNodeHelper(args, nullptr, &node))
    return;

  int index = args[2]->Int32Value();
  if (index < 0 || index >= node->child_count())
    return;

  int child_id = node->children()[index]->id();
  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), child_id));
}

void AutomationInternalCustomBindings::GetIndexInParent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  if (!GetNodeHelper(args, nullptr, &node))
    return;

  int index_in_parent = node->index_in_parent();
  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), index_in_parent));
}

void AutomationInternalCustomBindings::GetState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  if (!GetNodeHelper(args, nullptr, &node))
    return;

  v8::Local<v8::Object> state(v8::Object::New(GetIsolate()));
  uint32 state_pos = 0, state_shifter = node->data().state;
  while (state_shifter) {
    if (state_shifter & 1) {
      std::string key = ToString(static_cast<ui::AXState>(state_pos));
      state->Set(CreateV8String(key),
                 v8::Boolean::New(GetIsolate(), true));
    }
    state_shifter = state_shifter >> 1;
    state_pos++;
  }

  args.GetReturnValue().Set(state);
}

void AutomationInternalCustomBindings::GetRole(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  if (!GetNodeHelper(args, nullptr, &node))
    return;

  std::string role_name = ui::ToString(node->data().role);
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(GetIsolate(), role_name.c_str()));
}

void AutomationInternalCustomBindings::GetLocation(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  TreeCache* cache;
  ui::AXNode* node = nullptr;
  if (!GetNodeHelper(args, &cache, &node))
    return;

  v8::Local<v8::Object> location_obj(v8::Object::New(GetIsolate()));
  gfx::Rect location = node->data().location;
  location.Offset(cache->location_offset);
  location_obj->Set(CreateV8String("left"),
                    v8::Integer::New(GetIsolate(), location.x()));
  location_obj->Set(CreateV8String("top"),
                    v8::Integer::New(GetIsolate(), location.y()));
  location_obj->Set(CreateV8String("width"),
                    v8::Integer::New(GetIsolate(), location.width()));
  location_obj->Set(CreateV8String("height"),
                    v8::Integer::New(GetIsolate(), location.height()));
  args.GetReturnValue().Set(location_obj);
}

void AutomationInternalCustomBindings::GetStringAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  std::string attribute_name;
  if (!GetAttributeHelper(args, &node, &attribute_name))
    return;

  ui::AXStringAttribute attribute = ui::ParseAXStringAttribute(attribute_name);
  std::string attr_value;
  if (!node->data().GetStringAttribute(attribute, &attr_value))
    return;

  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(GetIsolate(), attr_value.c_str()));
}

void AutomationInternalCustomBindings::GetBoolAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  std::string attribute_name;
  if (!GetAttributeHelper(args, &node, &attribute_name))
    return;

  ui::AXBoolAttribute attribute = ui::ParseAXBoolAttribute(attribute_name);
  bool attr_value;
  if (!node->data().GetBoolAttribute(attribute, &attr_value))
    return;

  args.GetReturnValue().Set(v8::Boolean::New(GetIsolate(), attr_value));
}

void AutomationInternalCustomBindings::GetIntAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  std::string attribute_name;
  if (!GetAttributeHelper(args, &node, &attribute_name))
    return;

  ui::AXIntAttribute attribute = ui::ParseAXIntAttribute(attribute_name);
  int attr_value;
  if (!node->data().GetIntAttribute(attribute, &attr_value))
    return;

  args.GetReturnValue().Set(v8::Integer::New(GetIsolate(), attr_value));
}

void AutomationInternalCustomBindings::GetFloatAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  std::string attribute_name;
  if (!GetAttributeHelper(args, &node, &attribute_name))
    return;

  ui::AXFloatAttribute attribute = ui::ParseAXFloatAttribute(attribute_name);
  float attr_value;

  if (!node->data().GetFloatAttribute(attribute, &attr_value))
    return;

  args.GetReturnValue().Set(v8::Number::New(GetIsolate(), attr_value));
}

void AutomationInternalCustomBindings::GetIntListAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  std::string attribute_name;
  if (!GetAttributeHelper(args, &node, &attribute_name))
    return;

  ui::AXIntListAttribute attribute =
      ui::ParseAXIntListAttribute(attribute_name);
  if (!node->data().HasIntListAttribute(attribute))
    return;
  const std::vector<int32>& attr_value =
      node->data().GetIntListAttribute(attribute);

  v8::Local<v8::Array> result(v8::Array::New(GetIsolate(), attr_value.size()));
  for (size_t i = 0; i < attr_value.size(); ++i)
    result->Set(static_cast<uint32>(i),
                v8::Integer::New(GetIsolate(), attr_value[i]));
  args.GetReturnValue().Set(result);
}

void AutomationInternalCustomBindings::GetHtmlAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AXNode* node = nullptr;
  std::string attribute_name;
  if (!GetAttributeHelper(args, &node, &attribute_name))
    return;

  std::string attr_value;
  if (!node->data().GetHtmlAttribute(attribute_name.c_str(), &attr_value))
    return;

  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(GetIsolate(), attr_value.c_str()));
}

//
// Helper functions.
//

void AutomationInternalCustomBindings::ThrowInvalidArgumentsException(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  GetIsolate()->ThrowException(
      v8::String::NewFromUtf8(
          GetIsolate(),
          "Invalid arguments to AutomationInternalCustomBindings function",
          v8::NewStringType::kNormal).ToLocalChecked());

  LOG(FATAL)
      << "Invalid arguments to AutomationInternalCustomBindings function"
      << context()->GetStackTraceAsString();
}

bool AutomationInternalCustomBindings::GetNodeHelper(
    const v8::FunctionCallbackInfo<v8::Value>& args,
    TreeCache** out_cache,
    ui::AXNode** out_node) {
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber()) {
    ThrowInvalidArgumentsException(args);
    return false;
  }

  int tree_id = args[0]->Int32Value();
  int node_id = args[1]->Int32Value();

  const auto iter = tree_id_to_tree_cache_map_.find(tree_id);
  if (iter == tree_id_to_tree_cache_map_.end())
    return false;

  TreeCache* cache = iter->second;
  ui::AXNode* node = cache->tree.GetFromId(node_id);

  if (out_cache)
    *out_cache = cache;
  if (out_node)
    *out_node = node;

  return node != nullptr;
}

bool AutomationInternalCustomBindings::GetAttributeHelper(
    const v8::FunctionCallbackInfo<v8::Value>& args,
    ui::AXNode** out_node,
    std::string* out_attribute_name) {
  if (args.Length() != 3 ||
      !args[2]->IsString()) {
    ThrowInvalidArgumentsException(args);
    return false;
  }

  TreeCache* cache = nullptr;
  if (!GetNodeHelper(args, &cache, out_node))
    return false;

  *out_attribute_name = *v8::String::Utf8Value(args[2]);
  return true;
}

v8::Local<v8::Value> AutomationInternalCustomBindings::CreateV8String(
    const char* str) {
  return v8::String::NewFromUtf8(
      GetIsolate(), str, v8::String::kNormalString, strlen(str));
}

v8::Local<v8::Value> AutomationInternalCustomBindings::CreateV8String(
    const std::string& str) {
  return v8::String::NewFromUtf8(
      GetIsolate(), str.c_str(), v8::String::kNormalString, str.length());
}

//
// Handle accessibility events from the browser process.
//

void AutomationInternalCustomBindings::OnAccessibilityEvent(
    const ExtensionMsg_AccessibilityEventParams& params) {
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

  cache->location_offset = params.location_offset;
  if (!cache->tree.Unserialize(params.update)) {
    LOG(FATAL) << cache->tree.error();
    return;
  }

  v8::HandleScope handle_scope(GetIsolate());
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> args(v8::Array::New(GetIsolate(), 1U));
  v8::Local<v8::Object> event_params(v8::Object::New(GetIsolate()));
  event_params->Set(CreateV8String("treeID"),
                    v8::Integer::New(GetIsolate(), params.tree_id));
  event_params->Set(CreateV8String("targetID"),
                    v8::Integer::New(GetIsolate(), params.id));
  event_params->Set(CreateV8String("eventType"),
                    CreateV8String(ToString(params.event_type)));
  args->Set(0U, event_params);
  context()->DispatchEvent("automationInternal.onAccessibilityEvent", args);
}

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
  auto iter = axtree_to_tree_cache_map_.find(tree);
  if (iter == axtree_to_tree_cache_map_.end())
    return;

  int tree_id = iter->second->tree_id;

  v8::HandleScope handle_scope(GetIsolate());
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> args(v8::Array::New(GetIsolate(), 3U));
  args->Set(0U, v8::Integer::New(GetIsolate(), tree_id));
  args->Set(1U, v8::Integer::New(GetIsolate(), node->id()));
  args->Set(2U, CreateV8String(ToString(change_type)));
  context()->DispatchEvent("automationInternal.onTreeChange", args);
}

}  // namespace extensions
