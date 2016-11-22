// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/extensions/api/automation_api_constants.h"
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
#include "ui/gfx/geometry/rect_conversions.h"

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

v8::Local<v8::Object> RectToV8Object(v8::Isolate* isolate,
                                     const gfx::Rect& rect) {
  v8::Local<v8::Object> result(v8::Object::New(isolate));
  result->Set(CreateV8String(isolate, "left"),
              v8::Integer::New(isolate, rect.x()));
  result->Set(CreateV8String(isolate, "top"),
              v8::Integer::New(isolate, rect.y()));
  result->Set(CreateV8String(isolate, "width"),
              v8::Integer::New(isolate, rect.width()));
  result->Set(CreateV8String(isolate, "height"),
              v8::Integer::New(isolate, rect.height()));
  return result;
}

// Compute the bounding box of a node, fixing nodes with empty bounds by
// unioning the bounds of their children.
static gfx::RectF ComputeLocalNodeBounds(TreeCache* cache, ui::AXNode* node) {
  gfx::RectF bounds = node->data().location;
  if (bounds.width() > 0 && bounds.height() > 0)
    return bounds;

  // Compute the bounds of each child.
  for (size_t i = 0; i < node->children().size(); i++) {
    ui::AXNode* child = node->children()[i];
    gfx::RectF child_bounds = ComputeLocalNodeBounds(cache, child);

    // Ignore children that don't have valid bounds themselves.
    if (child_bounds.width() == 0 || child_bounds.height() == 0)
      continue;

    // For the first valid child, just set the bounds to that child's bounds.
    if (bounds.width() == 0 || bounds.height() == 0) {
      bounds = child_bounds;
      continue;
    }

    // Union each additional child's bounds.
    bounds.Union(child_bounds);
  }

  return bounds;
}

// Adjust the bounding box of a node from local to global coordinates,
// walking up the parent hierarchy to offset by frame offsets and
// scroll offsets.
static gfx::Rect ComputeGlobalNodeBounds(TreeCache* cache,
                                         ui::AXNode* node,
                                         gfx::RectF local_bounds) {
  gfx::RectF bounds = local_bounds;
  while (node) {
    if (node->data().transform)
      node->data().transform->TransformRect(&bounds);

    ui::AXNode* container =
        cache->tree.GetFromId(node->data().offset_container_id);
    if (!container) {
      if (node == cache->tree.root()) {
        container = cache->owner->GetParent(node, &cache);
      } else {
        container = cache->tree.root();
      }
    }

    if (!container || container == node)
      break;

    gfx::RectF container_bounds = container->data().location;
    bounds.Offset(container_bounds.x(), container_bounds.y());

    int scroll_x = 0;
    int scroll_y = 0;
    if (container->data().GetIntAttribute(ui::AX_ATTR_SCROLL_X, &scroll_x) &&
        container->data().GetIntAttribute(ui::AX_ATTR_SCROLL_Y, &scroll_y)) {
      bounds.Offset(-scroll_x, -scroll_y);
    }

    node = container;
  }

  // All trees other than the desktop tree are scaled by the device
  // scale factor. Unscale them so they're all in consistent units.
  if (cache->tree_id != api::automation::kDesktopTreeID) {
    float scale_factor = cache->owner->context()
                             ->GetRenderFrame()
                             ->GetRenderView()
                             ->GetDeviceScaleFactor();
    if (scale_factor > 0)
      bounds.Scale(1.0 / scale_factor);
  }

  return gfx::ToEnclosingRect(bounds);
}

ui::AXNode* FindNodeWithChildTreeId(ui::AXNode* node, int child_tree_id) {
  if (child_tree_id == node->data().GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID))
    return node;

  for (int i = 0; i < node->child_count(); ++i) {
    ui::AXNode* result =
        FindNodeWithChildTreeId(node->ChildAtIndex(i), child_tree_id);
    if (result)
      return result;
  }

  return nullptr;
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

//
// Helper class that helps implement bindings for a JavaScript function
// that takes four input arguments: a tree ID, node ID, and integer start
// and end indices. Looks up the TreeCache and the AXNode and passes them
// to the function passed to the constructor.
//

typedef void (*NodeIDPlusRangeFunction)(v8::Isolate* isolate,
                                        v8::ReturnValue<v8::Value> result,
                                        TreeCache* cache,
                                        ui::AXNode* node,
                                        int start,
                                        int end);

class NodeIDPlusRangeWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusRangeWrapper> {
 public:
  NodeIDPlusRangeWrapper(AutomationInternalCustomBindings* automation_bindings,
                         NodeIDPlusRangeFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 4 || !args[0]->IsNumber() || !args[1]->IsNumber() ||
        !args[2]->IsNumber() || !args[3]->IsNumber()) {
      ThrowInvalidArgumentsException(automation_bindings_);
    }

    int tree_id = args[0]->Int32Value();
    int node_id = args[1]->Int32Value();
    int start = args[2]->Int32Value();
    int end = args[3]->Int32Value();

    TreeCache* cache = automation_bindings_->GetTreeCacheFromTreeID(tree_id);
    if (!cache)
      return;

    ui::AXNode* node = cache->tree.GetFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), cache, node, start, end);
  }

 private:
  virtual ~NodeIDPlusRangeWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusRangeWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusRangeFunction function_;
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
    : ObjectBackedNativeHandler(context),
      is_active_profile_(true),
      tree_change_observer_overall_filter_(0) {
  // It's safe to use base::Unretained(this) here because these bindings
  // will only be called on a valid AutomationInternalCustomBindings instance
  // and none of the functions have any side effects.
#define ROUTE_FUNCTION(FN)                                        \
  RouteFunction(#FN, "automation",                                \
                base::Bind(&AutomationInternalCustomBindings::FN, \
                           base::Unretained(this)))
  ROUTE_FUNCTION(IsInteractPermitted);
  ROUTE_FUNCTION(GetSchemaAdditions);
  ROUTE_FUNCTION(GetRoutingID);
  ROUTE_FUNCTION(StartCachingAccessibilityTrees);
  ROUTE_FUNCTION(DestroyAccessibilityTree);
  ROUTE_FUNCTION(AddTreeChangeObserver);
  ROUTE_FUNCTION(RemoveTreeChangeObserver);
  ROUTE_FUNCTION(GetChildIDAtIndex);
  ROUTE_FUNCTION(GetFocus);
  ROUTE_FUNCTION(GetHtmlAttributes);
  ROUTE_FUNCTION(GetState);
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
  RouteTreeIDFunction("GetAnchorAffinity", [](v8::Isolate* isolate,
                                           v8::ReturnValue<v8::Value> result,
                                           TreeCache* cache) {
    result.Set(CreateV8String(isolate,
          ToString(cache->tree.data().sel_anchor_affinity)));
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
  RouteTreeIDFunction("GetFocusAffinity", [](v8::Isolate* isolate,
                                           v8::ReturnValue<v8::Value> result,
                                           TreeCache* cache) {
    result.Set(CreateV8String(isolate,
          ToString(cache->tree.data().sel_focus_affinity)));
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
      "GetRole", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                    TreeCache* cache, ui::AXNode* node) {
        std::string role_name = ui::ToString(node->data().role);
        result.Set(v8::String::NewFromUtf8(isolate, role_name.c_str()));
      });
  RouteNodeIDFunction(
      "GetLocation", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                        TreeCache* cache, ui::AXNode* node) {
        gfx::RectF local_bounds = ComputeLocalNodeBounds(cache, node);
        gfx::Rect global_bounds =
            ComputeGlobalNodeBounds(cache, node, local_bounds);
        global_bounds.Offset(cache->location_offset);
        result.Set(RectToV8Object(isolate, global_bounds));
      });
  RouteNodeIDFunction(
      "GetLineStartOffsets",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         TreeCache* cache, ui::AXNode* node) {
        const std::vector<int> line_starts =
            node->GetOrComputeLineStartOffsets();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, line_starts.size()));
        for (size_t i = 0; i < line_starts.size(); ++i) {
          array_result->Set(static_cast<uint32_t>(i),
                            v8::Integer::New(isolate, line_starts[i]));
        }
        result.Set(array_result);
      });
  RouteNodeIDFunction("GetChildIDs", [](v8::Isolate* isolate,
                                        v8::ReturnValue<v8::Value> result,
                                        TreeCache* cache, ui::AXNode* node) {
    const std::vector<ui::AXNode*>& children = node->children();
    v8::Local<v8::Array> array_result(v8::Array::New(isolate, children.size()));
    for (size_t i = 0; i < children.size(); ++i) {
      array_result->Set(static_cast<uint32_t>(i),
                        v8::Integer::New(isolate, children[i]->id()));
    }
    result.Set(array_result);
  });

  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusRangeFunction(
      "GetBoundsForRange",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         TreeCache* cache, ui::AXNode* node, int start, int end) {
        gfx::RectF local_bounds = ComputeLocalNodeBounds(cache, node);
        if (node->data().role != ui::AX_ROLE_INLINE_TEXT_BOX) {
          gfx::Rect global_bounds =
              ComputeGlobalNodeBounds(cache, node, local_bounds);
          global_bounds.Offset(cache->location_offset);
          result.Set(RectToV8Object(isolate, global_bounds));
        }

        // Use character offsets to compute the local bounds of this subrange.
        std::string name = node->data().GetStringAttribute(ui::AX_ATTR_NAME);
        std::vector<int> character_offsets =
            node->data().GetIntListAttribute(ui::AX_ATTR_CHARACTER_OFFSETS);
        int len =
            static_cast<int>(std::min(name.size(), character_offsets.size()));
        if (start >= 0 && start <= end && end <= len) {
          int start_offset = start > 0 ? character_offsets[start - 1] : 0;
          int end_offset = end > 0 ? character_offsets[end - 1] : 0;

          switch (node->data().GetIntAttribute(ui::AX_ATTR_TEXT_DIRECTION)) {
            case ui::AX_TEXT_DIRECTION_LTR:
            default:
              local_bounds.set_x(local_bounds.x() + start_offset);
              local_bounds.set_width(end_offset - start_offset);
              break;
            case ui::AX_TEXT_DIRECTION_RTL:
              local_bounds.set_x(local_bounds.x() + local_bounds.width() -
                                 end_offset);
              local_bounds.set_width(end_offset - start_offset);
              break;
            case ui::AX_TEXT_DIRECTION_TTB:
              local_bounds.set_y(local_bounds.y() + start_offset);
              local_bounds.set_height(end_offset - start_offset);
              break;
            case ui::AX_TEXT_DIRECTION_BTT:
              local_bounds.set_y(local_bounds.y() + local_bounds.height() -
                                 end_offset);
              local_bounds.set_height(end_offset - start_offset);
              break;
          }
        }

        // Convert from local to global coordinates second, after subsetting,
        // because the local to global conversion might involve matrix
        // transformations.
        gfx::Rect global_bounds =
            ComputeGlobalNodeBounds(cache, node, local_bounds);
        global_bounds.Offset(cache->location_offset);
        result.Set(RectToV8Object(isolate, global_bounds));
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
        const std::vector<int32_t>& attr_value =
            node->data().GetIntListAttribute(attribute);

        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, attr_value.size()));
        for (size_t i = 0; i < attr_value.size(); ++i)
          array_result->Set(static_cast<uint32_t>(i),
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

AutomationInternalCustomBindings::~AutomationInternalCustomBindings() {}

void AutomationInternalCustomBindings::Invalidate() {
  ObjectBackedNativeHandler::Invalidate();

  if (message_filter_)
    message_filter_->Detach();

  // Delete the TreeCaches quickly by first clearing their delegates so
  // we don't get a callback for every node being deleted.
  for (auto iter : tree_id_to_tree_cache_map_) {
    TreeCache* cache = iter.second;
    cache->tree.SetDelegate(nullptr);
    delete cache;
  }
  tree_id_to_tree_cache_map_.clear();
}

void AutomationInternalCustomBindings::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AutomationInternalCustomBindings, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityEvent, OnAccessibilityEvent)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityLocationChange,
                        OnAccessibilityLocationChange)
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

  v8::Local<v8::Object> name_from_type(v8::Object::New(GetIsolate()));
  for (int i = ui::AX_NAME_FROM_NONE; i <= ui::AX_NAME_FROM_LAST; ++i) {
    name_from_type->Set(
        v8::Integer::New(GetIsolate(), i),
        CreateV8String(GetIsolate(),
                       ui::ToString(static_cast<ui::AXNameFrom>(i))));
  }

  additions->Set(v8::String::NewFromUtf8(GetIsolate(), "NameFromType"),
                 name_from_type);

  v8::Local<v8::Object> description_from_type(v8::Object::New(GetIsolate()));
  for (int i = ui::AX_DESCRIPTION_FROM_NONE; i <= ui::AX_DESCRIPTION_FROM_LAST;
       ++i) {
    description_from_type->Set(
        v8::Integer::New(GetIsolate(), i),
        CreateV8String(GetIsolate(),
                       ui::ToString(static_cast<ui::AXDescriptionFrom>(i))));
  }

  additions->Set(v8::String::NewFromUtf8(GetIsolate(), "DescriptionFromType"),
                 description_from_type);

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

void AutomationInternalCustomBindings::AddTreeChangeObserver(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2 || !args[0]->IsNumber() || !args[1]->IsString()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  TreeChangeObserver observer;
  observer.id = args[0]->Int32Value();
  std::string filter_str = *v8::String::Utf8Value(args[1]);
  observer.filter = api::automation::ParseTreeChangeObserverFilter(filter_str);

  tree_change_observers_.push_back(observer);
  UpdateOverallTreeChangeObserverFilter();
}

void AutomationInternalCustomBindings::RemoveTreeChangeObserver(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // The argument is an integer key for an object which is automatically
  // converted to a string.
  if (args.Length() != 1 || !args[0]->IsString()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  int observer_id = args[0]->Int32Value();

  for (auto iter = tree_change_observers_.begin();
       iter != tree_change_observers_.end(); ++iter) {
    if (iter->id == observer_id) {
      tree_change_observers_.erase(iter);
      break;
    }
  }

  UpdateOverallTreeChangeObserverFilter();
}

bool AutomationInternalCustomBindings::GetFocusInternal(TreeCache* cache,
                                                        TreeCache** out_cache,
                                                        ui::AXNode** out_node) {
  int focus_id = cache->tree.data().focus_id;
  ui::AXNode* focus = cache->tree.GetFromId(focus_id);
  if (!focus)
    return false;

  // If the focused node is the owner of a child tree, that indicates
  // a node within the child tree is the one that actually has focus.
  while (focus->data().HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID)) {
    // Try to keep following focus recursively, by letting |tree_id| be the
    // new subtree to search in, while keeping |focus_tree_id| set to the tree
    // where we know we found a focused node.
    int child_tree_id =
        focus->data().GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID);

    TreeCache* child_cache = GetTreeCacheFromTreeID(child_tree_id);
    if (!child_cache)
      break;

    // If the child cache is a frame tree that indicates a focused frame,
    // jump to that frame if possible.
    if (child_cache->tree.data().focused_tree_id > 0) {
      TreeCache* focused_cache =
          GetTreeCacheFromTreeID(child_cache->tree.data().focused_tree_id);
      if (focused_cache)
        child_cache = focused_cache;
    }

    int child_focus_id = child_cache->tree.data().focus_id;
    ui::AXNode* child_focus = child_cache->tree.GetFromId(child_focus_id);
    if (!child_focus)
      break;

    focus = child_focus;
    cache = child_cache;
  }

  *out_cache = cache;
  *out_node = focus;
  return true;
}

void AutomationInternalCustomBindings::GetFocus(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsNumber()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  int tree_id = args[0]->Int32Value();
  TreeCache* cache = GetTreeCacheFromTreeID(tree_id);
  if (!cache)
    return;

  TreeCache* focused_tree_cache = nullptr;
  ui::AXNode* focused_node = nullptr;
  if (!GetFocusInternal(cache, &focused_tree_cache, &focused_node))
    return;

  v8::Isolate* isolate = GetIsolate();
  v8::Local<v8::Object> result(v8::Object::New(isolate));
  result->Set(CreateV8String(isolate, "treeId"),
              v8::Integer::New(isolate, focused_tree_cache->tree_id));
  result->Set(CreateV8String(isolate, "nodeId"),
              v8::Integer::New(isolate, focused_node->id()));
  args.GetReturnValue().Set(result);
}

void AutomationInternalCustomBindings::GetHtmlAttributes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber())
    ThrowInvalidArgumentsException(this);

  int tree_id = args[0]->Int32Value();
  int node_id = args[1]->Int32Value();

  TreeCache* cache = GetTreeCacheFromTreeID(tree_id);
  if (!cache)
    return;

  ui::AXNode* node = cache->tree.GetFromId(node_id);
  if (!node)
    return;

  v8::Local<v8::Object> dst(v8::Object::New(isolate));
  base::StringPairs src = node->data().html_attributes;
  for (size_t i = 0; i < src.size(); i++) {
    std::string& key = src[i].first;
    std::string& value = src[i].second;
    dst->Set(CreateV8String(isolate, key), CreateV8String(isolate, value));
  }
  args.GetReturnValue().Set(dst);
}

void AutomationInternalCustomBindings::GetState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber())
    ThrowInvalidArgumentsException(this);

  int tree_id = args[0]->Int32Value();
  int node_id = args[1]->Int32Value();

  TreeCache* cache = GetTreeCacheFromTreeID(tree_id);
  if (!cache)
    return;

  ui::AXNode* node = cache->tree.GetFromId(node_id);
  if (!node)
    return;

  v8::Local<v8::Object> state(v8::Object::New(isolate));
  uint32_t state_pos = 0, state_shifter = node->data().state;
  while (state_shifter) {
    if (state_shifter & 1) {
      std::string key = ToString(static_cast<ui::AXState>(state_pos));
      state->Set(CreateV8String(isolate, key), v8::Boolean::New(isolate, true));
    }
    state_shifter = state_shifter >> 1;
    state_pos++;
  }

  TreeCache* top_cache = GetTreeCacheFromTreeID(0);
  if (!top_cache)
    top_cache = cache;
  TreeCache* focused_cache = nullptr;
  ui::AXNode* focused_node = nullptr;
  if (GetFocusInternal(top_cache, &focused_cache, &focused_node)) {
    if (focused_cache == cache && focused_node == node) {
      state->Set(CreateV8String(isolate, "focused"),
                 v8::Boolean::New(isolate, true));
    }
  }
  if (cache->tree.data().focus_id == node->id()) {
    state->Set(CreateV8String(isolate, "focused"),
               v8::Boolean::New(isolate, true));
  }

  args.GetReturnValue().Set(state);
}

void AutomationInternalCustomBindings::UpdateOverallTreeChangeObserverFilter() {
  tree_change_observer_overall_filter_ = 0;
  for (const auto& observer : tree_change_observers_)
    tree_change_observer_overall_filter_ |= 1 << observer.filter;
}

ui::AXNode* AutomationInternalCustomBindings::GetParent(
    ui::AXNode* node,
    TreeCache** in_out_cache) {
  if (node->parent())
    return node->parent();

  int parent_tree_id = (*in_out_cache)->tree.data().parent_tree_id;
  if (parent_tree_id < 0)
    return nullptr;

  TreeCache* parent_cache = GetTreeCacheFromTreeID(parent_tree_id);
  if (!parent_cache)
    return nullptr;

  // Try to use the cached parent node from the most recent time this
  // was called.
  if (parent_cache->parent_node_id_from_parent_tree > 0) {
    ui::AXNode* parent = parent_cache->tree.GetFromId(
        parent_cache->parent_node_id_from_parent_tree);
    if (parent) {
      int parent_child_tree_id =
          parent->data().GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID);
      if (parent_child_tree_id == (*in_out_cache)->tree_id) {
        *in_out_cache = parent_cache;
        return parent;
      }
    }
  }

  // If that fails, search for it and cache it for next time.
  ui::AXNode* parent = FindNodeWithChildTreeId(parent_cache->tree.root(),
                                               (*in_out_cache)->tree_id);
  if (parent) {
    (*in_out_cache)->parent_node_id_from_parent_tree = parent->id();
    *in_out_cache = parent_cache;
    return parent;
  }

  return nullptr;
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

void AutomationInternalCustomBindings::RouteNodeIDPlusRangeFunction(
    const std::string& name,
    NodeIDPlusRangeFunction callback) {
  scoped_refptr<NodeIDPlusRangeWrapper> wrapper =
      new NodeIDPlusRangeWrapper(this, callback);
  RouteFunction(name, base::Bind(&NodeIDPlusRangeWrapper::Run, wrapper));
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
    cache->parent_node_id_from_parent_tree = -1;
    cache->tree.SetDelegate(this);
    cache->owner = this;
    tree_id_to_tree_cache_map_.insert(std::make_pair(tree_id, cache));
    axtree_to_tree_cache_map_.insert(std::make_pair(&cache->tree, cache));
  } else {
    cache = iter->second;
  }

  // Update the internal state whether it's the active profile or not.
  cache->location_offset = params.location_offset;
  deleted_node_ids_.clear();
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> args(v8::Array::New(GetIsolate(), 1U));
  if (!cache->tree.Unserialize(params.update)) {
    LOG(ERROR) << cache->tree.error();
    args->Set(0U, v8::Number::New(isolate, tree_id));
    context()->DispatchEvent(
        "automationInternal.onAccessibilityTreeSerializationError", args);
    return;
  }

  // Don't send any events if it's not the active profile.
  if (!is_active_profile)
    return;

  SendNodesRemovedEvent(&cache->tree, deleted_node_ids_);
  deleted_node_ids_.clear();

  v8::Local<v8::Object> event_params(v8::Object::New(GetIsolate()));
  event_params->Set(CreateV8String(isolate, "treeID"),
                    v8::Integer::New(GetIsolate(), params.tree_id));
  event_params->Set(CreateV8String(isolate, "targetID"),
                    v8::Integer::New(GetIsolate(), params.id));
  event_params->Set(CreateV8String(isolate, "eventType"),
                    CreateV8String(isolate, ToString(params.event_type)));
  event_params->Set(CreateV8String(isolate, "eventFrom"),
                    CreateV8String(isolate, ToString(params.event_from)));
  event_params->Set(CreateV8String(isolate, "mouseX"),
                    v8::Integer::New(GetIsolate(), params.mouse_location.x()));
  event_params->Set(CreateV8String(isolate, "mouseY"),
                    v8::Integer::New(GetIsolate(), params.mouse_location.y()));
  args->Set(0U, event_params);
  context()->DispatchEvent("automationInternal.onAccessibilityEvent", args);
}

void AutomationInternalCustomBindings::OnAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  int tree_id = params.tree_id;
  auto iter = tree_id_to_tree_cache_map_.find(tree_id);
  if (iter == tree_id_to_tree_cache_map_.end())
    return;
  TreeCache* cache = iter->second;
  ui::AXNode* node = cache->tree.GetFromId(params.id);
  if (!node)
    return;
  node->SetLocation(params.new_location.offset_container_id,
                    params.new_location.bounds,
                    params.new_location.transform.get());
}

void AutomationInternalCustomBindings::OnNodeDataWillChange(
    ui::AXTree* tree,
    const ui::AXNodeData& old_node_data,
    const ui::AXNodeData& new_node_data) {
  if (old_node_data.GetStringAttribute(ui::AX_ATTR_NAME) !=
      new_node_data.GetStringAttribute(ui::AX_ATTR_NAME))
    text_changed_node_ids_.push_back(new_node_data.id);
}

void AutomationInternalCustomBindings::OnTreeDataChanged(ui::AXTree* tree) {}

void AutomationInternalCustomBindings::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                           ui::AXNode* node) {
  SendTreeChangeEvent(
      api::automation::TREE_CHANGE_TYPE_NODEREMOVED,
      tree, node);
  deleted_node_ids_.push_back(node->id());
}

void AutomationInternalCustomBindings::OnSubtreeWillBeDeleted(
    ui::AXTree* tree,
    ui::AXNode* node) {
  // This isn't strictly needed, as OnNodeWillBeDeleted will already be
  // called. We could send a JS event for this only if it turns out to
  // be needed for something.
}

void AutomationInternalCustomBindings::OnNodeWillBeReparented(
    ui::AXTree* tree,
    ui::AXNode* node) {
  // Don't do anything here since the node will soon go away and be re-created.
}

void AutomationInternalCustomBindings::OnSubtreeWillBeReparented(
    ui::AXTree* tree,
    ui::AXNode* node) {
  // Don't do anything here since the node will soon go away and be re-created.
}

void AutomationInternalCustomBindings::OnNodeCreated(ui::AXTree* tree,
                                                     ui::AXNode* node) {
  // Not needed, this is called in the middle of an update so it's not
  // safe to trigger JS from here. Wait for the notification in
  // OnAtomicUpdateFinished instead.
}

void AutomationInternalCustomBindings::OnNodeReparented(ui::AXTree* tree,
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

  for (const auto change : changes) {
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
      // Unhandled.
      case NODE_REPARENTED:
      case SUBTREE_REPARENTED:
        break;
    }
  }

  for (int id : text_changed_node_ids_) {
    SendTreeChangeEvent(api::automation::TREE_CHANGE_TYPE_TEXTCHANGED, tree,
                        tree->GetFromId(id));
  }
  text_changed_node_ids_.clear();
}

void AutomationInternalCustomBindings::SendTreeChangeEvent(
    api::automation::TreeChangeType change_type,
    ui::AXTree* tree,
    ui::AXNode* node) {
  // Don't send tree change events when it's not the active profile.
  if (!is_active_profile_)
    return;

  // Always notify the custom bindings when there's a node with a child tree
  // ID that might need to be loaded.
  if (node->data().HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID))
    SendChildTreeIDEvent(tree, node);

  bool has_filter = false;
  if (tree_change_observer_overall_filter_ &
      (1 <<
       api::automation::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES)) {
    if (node->data().HasStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS) ||
        node->data().role == ui::AX_ROLE_ALERT) {
      has_filter = true;
    }
  }

  if (tree_change_observer_overall_filter_ &
      (1 << api::automation::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES)) {
    if (node->data().HasIntListAttribute(ui::AX_ATTR_MARKER_TYPES))
      has_filter = true;
  }

  if (tree_change_observer_overall_filter_ &
      (1 << api::automation::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES))
    has_filter = true;

  if (!has_filter)
    return;

  auto iter = axtree_to_tree_cache_map_.find(tree);
  if (iter == axtree_to_tree_cache_map_.end())
    return;

  int tree_id = iter->second->tree_id;

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  for (const auto& observer : tree_change_observers_) {
    switch (observer.filter) {
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_NOTREECHANGES:
      default:
        continue;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES:
        if (!node->data().HasStringAttribute(
                ui::AX_ATTR_CONTAINER_LIVE_STATUS) &&
            node->data().role != ui::AX_ROLE_ALERT) {
          continue;
        }
        break;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES:
        if (!node->data().HasIntListAttribute(ui::AX_ATTR_MARKER_TYPES))
          continue;
        break;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES:
        break;
    }

    v8::Local<v8::Array> args(v8::Array::New(GetIsolate(), 4U));
    args->Set(0U, v8::Integer::New(GetIsolate(), observer.id));
    args->Set(1U, v8::Integer::New(GetIsolate(), tree_id));
    args->Set(2U, v8::Integer::New(GetIsolate(), node->id()));
    args->Set(3U, CreateV8String(isolate, ToString(change_type)));
    context()->DispatchEvent("automationInternal.onTreeChange", args);
  }
}

void AutomationInternalCustomBindings::SendChildTreeIDEvent(ui::AXTree* tree,
                                                            ui::AXNode* node) {
  auto iter = axtree_to_tree_cache_map_.find(tree);
  if (iter == axtree_to_tree_cache_map_.end())
    return;

  int tree_id = iter->second->tree_id;

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> args(v8::Array::New(GetIsolate(), 2U));
  args->Set(0U, v8::Integer::New(GetIsolate(), tree_id));
  args->Set(1U, v8::Integer::New(GetIsolate(), node->id()));
  context()->DispatchEvent("automationInternal.onChildTreeID", args);
}

void AutomationInternalCustomBindings::SendNodesRemovedEvent(
    ui::AXTree* tree,
    const std::vector<int>& ids) {
  auto iter = axtree_to_tree_cache_map_.find(tree);
  if (iter == axtree_to_tree_cache_map_.end())
    return;

  int tree_id = iter->second->tree_id;

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> args(v8::Array::New(GetIsolate(), 2U));
  args->Set(0U, v8::Integer::New(GetIsolate(), tree_id));
  v8::Local<v8::Array> nodes(v8::Array::New(GetIsolate(), ids.size()));
  args->Set(1U, nodes);
  for (size_t i = 0; i < ids.size(); ++i)
    nodes->Set(i, v8::Integer::New(GetIsolate(), ids[i]));
  context()->DispatchEvent("automationInternal.onNodesRemoved", args);
}

}  // namespace extensions
