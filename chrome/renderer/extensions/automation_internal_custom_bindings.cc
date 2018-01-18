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
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "ipc/message_filter.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace extensions {

namespace {

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

v8::Local<v8::String> CreateV8String(v8::Isolate* isolate,
                                     base::StringPiece str) {
  return gin::StringToSymbol(isolate, str);
}

v8::Local<v8::Object> RectToV8Object(v8::Isolate* isolate,
                                     const gfx::Rect& rect) {
  return gin::DataObjectBuilder(isolate)
      .Set("left", rect.x())
      .Set("top", rect.y())
      .Set("width", rect.width())
      .Set("height", rect.height())
      .Build();
}

// Adjust the bounding box of a node from local to global coordinates,
// walking up the parent hierarchy to offset by frame offsets and
// scroll offsets.
// If |clip_bounds| is false, the bounds of the node will not be clipped
// to the ancestors bounding boxes if needed. Regardless of clipping, results
// are returned in global coordinates.
static gfx::Rect ComputeGlobalNodeBounds(AutomationAXTreeWrapper* tree_wrapper,
                                         ui::AXNode* node,
                                         gfx::RectF local_bounds = gfx::RectF(),
                                         bool* offscreen = nullptr,
                                         bool clip_bounds = true) {
  gfx::RectF bounds = local_bounds;

  while (node) {
    bounds = tree_wrapper->tree()->RelativeToTreeBounds(node, bounds, offscreen,
                                                        clip_bounds);

    AutomationAXTreeWrapper* previous_tree_wrapper = tree_wrapper;
    ui::AXNode* parent = tree_wrapper->owner()->GetParent(
        tree_wrapper->tree()->root(), &tree_wrapper);
    if (parent == node)
      break;

    // All trees other than the desktop tree are scaled by the device
    // scale factor. When crossing out of another tree into the desktop
    // tree, unscale the bounds by the device scale factor.
    if (previous_tree_wrapper->tree_id() != api::automation::kDesktopTreeID &&
        tree_wrapper->tree_id() == api::automation::kDesktopTreeID) {
      float scale_factor = tree_wrapper->owner()->GetDeviceScaleFactor();
      if (scale_factor > 0)
        bounds.Scale(1.0 / scale_factor);
    }

    node = parent;
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
// the AutomationAXTreeWrapper and passes it to the function passed to the
// constructor.
//

typedef void (*TreeIDFunction)(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               AutomationAXTreeWrapper* tree_wrapper);

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
    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    // The root can be null if this is called from an onTreeChange callback.
    if (!tree_wrapper->tree()->root())
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper);
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
// AutomationAXTreeWrapper and the AXNode and passes them to the function passed
// to the constructor.
//

typedef void (*NodeIDFunction)(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               AutomationAXTreeWrapper* tree_wrapper,
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

    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node);
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
// argument. Looks up the AutomationAXTreeWrapper and the AXNode and passes them
// to the function passed to the constructor.
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
    std::string attribute = *v8::String::Utf8Value(isolate, args[2]);

    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
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
// and end indices. Looks up the AutomationAXTreeWrapper and the AXNode and
// passes them to the function passed to the constructor.
//

typedef void (*NodeIDPlusRangeFunction)(v8::Isolate* isolate,
                                        v8::ReturnValue<v8::Value> result,
                                        AutomationAXTreeWrapper* tree_wrapper,
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

    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node, start, end);
  }

 private:
  virtual ~NodeIDPlusRangeWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusRangeWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusRangeFunction function_;
};

}  // namespace

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
    ScriptContext* context,
    ExtensionBindingsSystem* bindings_system)
    : ObjectBackedNativeHandler(context),
      is_active_profile_(true),
      tree_change_observer_overall_filter_(0),
      bindings_system_(bindings_system),
      should_ignore_context_(false) {
  // We will ignore this instance if the extension has a background page and
  // this context is not that background page. In all other cases, we will have
  // multiple instances floating around in the same process.
  if (context && context->extension()) {
    const GURL background_page_url =
        extensions::BackgroundInfo::GetBackgroundURL(context->extension());
    should_ignore_context_ = background_page_url != "" &&
        background_page_url != context->url();
  }

  // It's safe to use base::Unretained(this) here because these bindings
  // will only be called on a valid AutomationInternalCustomBindings instance
  // and none of the functions have any side effects.
#define ROUTE_FUNCTION(FN)                                        \
  RouteFunction(#FN, "automation",                                \
                base::Bind(&AutomationInternalCustomBindings::FN, \
                           base::Unretained(this)))
  ROUTE_FUNCTION(IsInteractPermitted);
  ROUTE_FUNCTION(GetSchemaAdditions);
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

  RouteTreeIDFunction("GetRootID", [](v8::Isolate* isolate,
                                      v8::ReturnValue<v8::Value> result,
                                      AutomationAXTreeWrapper* tree_wrapper) {
    result.Set(v8::Integer::New(isolate, tree_wrapper->tree()->root()->id()));
  });
  RouteTreeIDFunction(
      "GetDocURL", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::String::NewFromUtf8(
            isolate, tree_wrapper->tree()->data().url.c_str()));
      });
  RouteTreeIDFunction(
      "GetDocTitle", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                        AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::String::NewFromUtf8(
            isolate, tree_wrapper->tree()->data().title.c_str()));
      });
  RouteTreeIDFunction(
      "GetDocLoaded",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(
            v8::Boolean::New(isolate, tree_wrapper->tree()->data().loaded));
      });
  RouteTreeIDFunction(
      "GetDocLoadingProgress",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->tree()->data().loading_progress));
      });
  RouteTreeIDFunction(
      "GetAnchorObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->tree()->data().sel_anchor_object_id));
      });
  RouteTreeIDFunction(
      "GetAnchorOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->tree()->data().sel_anchor_offset));
      });
  RouteTreeIDFunction(
      "GetAnchorAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(CreateV8String(
            isolate,
            ToString(tree_wrapper->tree()->data().sel_anchor_affinity)));
      });
  RouteTreeIDFunction(
      "GetFocusObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->tree()->data().sel_focus_object_id));
      });
  RouteTreeIDFunction(
      "GetFocusOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->tree()->data().sel_focus_offset));
      });
  RouteTreeIDFunction(
      "GetFocusAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(CreateV8String(
            isolate,
            ToString(tree_wrapper->tree()->data().sel_focus_affinity)));
      });

  // Bindings that take a Tree ID and Node ID and return a property of the node.

  RouteNodeIDFunction(
      "GetParentID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->parent())
          result.Set(v8::Integer::New(isolate, node->parent()->id()));
      });
  RouteNodeIDFunction(
      "GetChildCount",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        result.Set(v8::Integer::New(isolate, node->child_count()));
      });
  RouteNodeIDFunction(
      "GetIndexInParent",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        result.Set(v8::Integer::New(isolate, node->index_in_parent()));
      });
  RouteNodeIDFunction(
      "GetRole", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                    AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::string role_name = ui::ToString(node->data().role);
        result.Set(v8::String::NewFromUtf8(isolate, role_name.c_str()));
      });
  RouteNodeIDFunction(
      "GetLocation",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        gfx::Rect global_clipped_bounds =
            ComputeGlobalNodeBounds(tree_wrapper, node);
        result.Set(RectToV8Object(isolate, global_clipped_bounds));
      });
  RouteNodeIDFunction(
      "GetUnclippedLocation",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool offscreen = false;
        gfx::Rect global_unclipped_bounds =
            ComputeGlobalNodeBounds(tree_wrapper, node, gfx::RectF(),
                                    &offscreen, false /* clip_bounds */);
        result.Set(RectToV8Object(isolate, global_unclipped_bounds));
      });
  RouteNodeIDFunction(
      "GetLineStartOffsets",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
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
                                        AutomationAXTreeWrapper* tree_wrapper,
                                        ui::AXNode* node) {
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
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node, int start,
         int end) {
        if (node->data().role != ui::AX_ROLE_INLINE_TEXT_BOX) {
          gfx::Rect global_bounds = ComputeGlobalNodeBounds(tree_wrapper, node);
          result.Set(RectToV8Object(isolate, global_bounds));
        }

        // Use character offsets to compute the local bounds of this subrange.
        gfx::RectF local_bounds(0, 0, node->data().location.width(),
                                node->data().location.height());
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
        // TODO(katie): Instead of removing clipping we could trim local_bounds
        // to fit in the clipped space?
        gfx::Rect global_bounds = ComputeGlobalNodeBounds(
            tree_wrapper, node, local_bounds, nullptr, false /* clip_bounds */);
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
  RouteNodeIDFunction(
      "GetNameFrom",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ui::AXNameFrom name_from = static_cast<ui::AXNameFrom>(
            node->data().GetIntAttribute(ui::AX_ATTR_NAME_FROM));
        std::string name_from_str = ui::ToString(name_from);
        result.Set(v8::String::NewFromUtf8(isolate, name_from_str.c_str()));
      });
  RouteNodeIDFunction(
      "GetBold", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                    AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = (node->data().GetIntAttribute(ui::AX_ATTR_TEXT_STYLE) &
                      ui::AX_TEXT_STYLE_BOLD) != 0;
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetItalic", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = (node->data().GetIntAttribute(ui::AX_ATTR_TEXT_STYLE) &
                      ui::AX_TEXT_STYLE_ITALIC) != 0;
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction("GetUnderline", [](v8::Isolate* isolate,
                                         v8::ReturnValue<v8::Value> result,
                                         AutomationAXTreeWrapper* tree_wrapper,
                                         ui::AXNode* node) {
    bool value = (node->data().GetIntAttribute(ui::AX_ATTR_TEXT_STYLE) &
                  ui::AX_TEXT_STYLE_UNDERLINE) != 0;
    result.Set(v8::Boolean::New(isolate, value));
  });
  RouteNodeIDFunction(
      "GetLineThrough",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = (node->data().GetIntAttribute(ui::AX_ATTR_TEXT_STYLE) &
                      ui::AX_TEXT_STYLE_LINE_THROUGH) != 0;
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetCustomActions",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::vector<int32_t>& custom_action_ids =
            node->data().GetIntListAttribute(ui::AX_ATTR_CUSTOM_ACTION_IDS);
        if (custom_action_ids.empty()) {
          result.SetUndefined();
          return;
        }

        const std::vector<std::string>& custom_action_descriptions =
            node->data().GetStringListAttribute(
                ui::AX_ATTR_CUSTOM_ACTION_DESCRIPTIONS);
        if (custom_action_ids.size() != custom_action_descriptions.size()) {
          NOTREACHED();
          return;
        }

        v8::Local<v8::Array> custom_actions(
            v8::Array::New(isolate, custom_action_ids.size()));
        for (size_t i = 0; i < custom_action_ids.size(); i++) {
          gin::DataObjectBuilder custom_action(isolate);
          custom_action.Set("id", custom_action_ids[i]);
          custom_action.Set("description", custom_action_descriptions[i]);
          custom_actions->Set(static_cast<uint32_t>(i), custom_action.Build());
        }
        result.Set(custom_actions);
      });
  RouteNodeIDFunction("GetChecked", [](v8::Isolate* isolate,
                                       v8::ReturnValue<v8::Value> result,
                                       AutomationAXTreeWrapper* tree_wrapper,
                                       ui::AXNode* node) {
    const ui::AXCheckedState checked_state = static_cast<ui::AXCheckedState>(
        node->data().GetIntAttribute(ui::AX_ATTR_CHECKED_STATE));
    if (checked_state) {
      const std::string checked_str = ui::ToString(checked_state);
      result.Set(v8::String::NewFromUtf8(isolate, checked_str.c_str()));
    }
  });
  RouteNodeIDFunction(
      "GetRestriction",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const ui::AXRestriction restriction = static_cast<ui::AXRestriction>(
            node->data().GetIntAttribute(ui::AX_ATTR_RESTRICTION));
        if (restriction) {
          const std::string restriction_str = ui::ToString(restriction);
          result.Set(v8::String::NewFromUtf8(isolate, restriction_str.c_str()));
        }
      });
}

AutomationInternalCustomBindings::~AutomationInternalCustomBindings() {}

void AutomationInternalCustomBindings::Invalidate() {
  ObjectBackedNativeHandler::Invalidate();

  if (message_filter_)
    message_filter_->Detach();

  tree_id_to_tree_wrapper_map_.clear();
}

// http://crbug.com/784266
// clang-format off
void AutomationInternalCustomBindings::OnMessageReceived(
    const IPC::Message& message){
  IPC_BEGIN_MESSAGE_MAP(AutomationInternalCustomBindings, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityEvent,
                        OnAccessibilityEvent)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityLocationChange,
                        OnAccessibilityLocationChange)
  IPC_END_MESSAGE_MAP()
}  // clang-format on

AutomationAXTreeWrapper* AutomationInternalCustomBindings::
    GetAutomationAXTreeWrapperFromTreeID(int tree_id) {
  const auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return nullptr;

  return iter->second.get();
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

void AutomationInternalCustomBindings::StartCachingAccessibilityTrees(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (should_ignore_context_)
    return;

  if (!message_filter_)
    message_filter_ = new AutomationMessageFilter(this);
}

void AutomationInternalCustomBindings::GetSchemaAdditions(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();

  gin::DataObjectBuilder name_from_type(isolate);
  for (int i = ui::AX_NAME_FROM_NONE; i <= ui::AX_NAME_FROM_LAST; ++i) {
    name_from_type.Set(
        i, base::StringPiece(ui::ToString(static_cast<ui::AXNameFrom>(i))));
  }

  gin::DataObjectBuilder restriction(isolate);
  for (int i = ui::AX_RESTRICTION_NONE; i <= ui::AX_RESTRICTION_LAST; ++i) {
    restriction.Set(
        i, base::StringPiece(ui::ToString(static_cast<ui::AXRestriction>(i))));
  }

  gin::DataObjectBuilder description_from_type(isolate);
  for (int i = ui::AX_DESCRIPTION_FROM_NONE; i <= ui::AX_DESCRIPTION_FROM_LAST;
       ++i) {
    description_from_type.Set(
        i,
        base::StringPiece(ui::ToString(static_cast<ui::AXDescriptionFrom>(i))));
  }

  args.GetReturnValue().Set(
      gin::DataObjectBuilder(isolate)
          .Set("NameFromType", name_from_type.Build())
          .Set("Restriction", restriction.Build())
          .Set("DescriptionFromType", description_from_type.Build())
          .Build());
}

void AutomationInternalCustomBindings::DestroyAccessibilityTree(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsNumber()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  int tree_id = args[0]->Int32Value();
  auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return;

  AutomationAXTreeWrapper* tree_wrapper = iter->second.get();
  axtree_to_tree_wrapper_map_.erase(tree_wrapper->tree());
  tree_id_to_tree_wrapper_map_.erase(tree_id);
}

void AutomationInternalCustomBindings::AddTreeChangeObserver(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2 || !args[0]->IsNumber() || !args[1]->IsString()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  TreeChangeObserver observer;
  observer.id = args[0]->Int32Value();
  std::string filter_str = *v8::String::Utf8Value(args.GetIsolate(), args[1]);
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

bool AutomationInternalCustomBindings::GetFocusInternal(
    AutomationAXTreeWrapper* tree_wrapper,
    AutomationAXTreeWrapper** out_tree_wrapper,
    ui::AXNode** out_node) {
  int focus_id = tree_wrapper->tree()->data().focus_id;
  ui::AXNode* focus = tree_wrapper->tree()->GetFromId(focus_id);
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

    AutomationAXTreeWrapper* child_tree_wrapper =
        GetAutomationAXTreeWrapperFromTreeID(child_tree_id);
    if (!child_tree_wrapper)
      break;

    // If |child_tree_wrapper| is a frame tree that indicates a focused frame,
    // jump to that frame if possible.
    if (child_tree_wrapper->tree()->data().focused_tree_id > 0) {
      AutomationAXTreeWrapper* focused_tree_wrapper =
          GetAutomationAXTreeWrapperFromTreeID(
              child_tree_wrapper->tree()->data().focused_tree_id);
      if (focused_tree_wrapper)
        child_tree_wrapper = focused_tree_wrapper;
    }

    int child_focus_id = child_tree_wrapper->tree()->data().focus_id;
    ui::AXNode* child_focus =
        child_tree_wrapper->tree()->GetFromId(child_focus_id);
    if (!child_focus)
      break;

    focus = child_focus;
    tree_wrapper = child_tree_wrapper;
  }

  *out_tree_wrapper = tree_wrapper;
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
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  AutomationAXTreeWrapper* focused_tree_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  if (!GetFocusInternal(tree_wrapper, &focused_tree_wrapper, &focused_node))
    return;

  args.GetReturnValue().Set(gin::DataObjectBuilder(GetIsolate())
                                .Set("treeId", focused_tree_wrapper->tree_id())
                                .Set("nodeId", focused_node->id())
                                .Build());
}

void AutomationInternalCustomBindings::GetHtmlAttributes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber())
    ThrowInvalidArgumentsException(this);

  int tree_id = args[0]->Int32Value();
  int node_id = args[1]->Int32Value();

  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
  if (!node)
    return;

  gin::DataObjectBuilder dst(isolate);
  for (const auto& pair : node->data().html_attributes)
    dst.Set(pair.first, pair.second);
  args.GetReturnValue().Set(dst.Build());
}

void AutomationInternalCustomBindings::GetState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber())
    ThrowInvalidArgumentsException(this);

  int tree_id = args[0]->Int32Value();
  int node_id = args[1]->Int32Value();

  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
  if (!node)
    return;

  gin::DataObjectBuilder state(isolate);
  uint32_t state_pos = 0, state_shifter = node->data().state;
  while (state_shifter) {
    if (state_shifter & 1)
      state.Set(ToString(static_cast<ui::AXState>(state_pos)), true);
    state_shifter = state_shifter >> 1;
    state_pos++;
  }

  AutomationAXTreeWrapper* top_tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(0);
  if (!top_tree_wrapper)
    top_tree_wrapper = tree_wrapper;
  AutomationAXTreeWrapper* focused_tree_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  const bool focused =
      (GetFocusInternal(top_tree_wrapper, &focused_tree_wrapper,
                        &focused_node) &&
       focused_tree_wrapper == tree_wrapper && focused_node == node) ||
      tree_wrapper->tree()->data().focus_id == node->id();
  if (focused)
    state.Set(ToString(api::automation::STATE_TYPE_FOCUSED), true);

  bool offscreen = false;
  ComputeGlobalNodeBounds(tree_wrapper, node, gfx::RectF(), &offscreen);
  if (offscreen)
    state.Set(ToString(api::automation::STATE_TYPE_OFFSCREEN), true);

  args.GetReturnValue().Set(state.Build());
}

void AutomationInternalCustomBindings::UpdateOverallTreeChangeObserverFilter() {
  tree_change_observer_overall_filter_ = 0;
  for (const auto& observer : tree_change_observers_)
    tree_change_observer_overall_filter_ |= 1 << observer.filter;
}

ui::AXNode* AutomationInternalCustomBindings::GetParent(
    ui::AXNode* node,
    AutomationAXTreeWrapper** in_out_tree_wrapper) {
  if (node->parent())
    return node->parent();

  int parent_tree_id = (*in_out_tree_wrapper)->tree()->data().parent_tree_id;

  // Try the desktop tree if the parent is unknown. If this tree really is
  // a child of the desktop tree, we'll find its parent, and if not, the
  // search, below, will fail until the real parent tree loads.
  if (parent_tree_id < 0)
    parent_tree_id = api::automation::kDesktopTreeID;

  AutomationAXTreeWrapper* parent_tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(parent_tree_id);
  if (!parent_tree_wrapper)
    return nullptr;

  // Try to use the cached host node from the most recent time this
  // was called.
  if ((*in_out_tree_wrapper)->host_node_id() > 0) {
    ui::AXNode* parent = parent_tree_wrapper->tree()->GetFromId(
        (*in_out_tree_wrapper)->host_node_id());
    if (parent) {
      int parent_child_tree_id =
          parent->data().GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID);
      if (parent_child_tree_id == (*in_out_tree_wrapper)->tree_id()) {
        *in_out_tree_wrapper = parent_tree_wrapper;
        return parent;
      }
    }
  }

  // If that fails, search for it and cache it for next time.
  ui::AXNode* parent = FindNodeWithChildTreeId(
      parent_tree_wrapper->tree()->root(), (*in_out_tree_wrapper)->tree_id());
  if (parent) {
    (*in_out_tree_wrapper)->set_host_node_id(parent->id());
    *in_out_tree_wrapper = parent_tree_wrapper;
    return parent;
  }

  return nullptr;
}

float AutomationInternalCustomBindings::GetDeviceScaleFactor() const {
  return context()->GetRenderFrame()->GetRenderView()->GetDeviceScaleFactor();
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

  const auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return;

  AutomationAXTreeWrapper* tree_wrapper = iter->second.get();
  ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
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
  AutomationAXTreeWrapper* tree_wrapper;
  auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end()) {
    tree_wrapper = new AutomationAXTreeWrapper(params.tree_id, this);
    tree_id_to_tree_wrapper_map_.insert(
        std::make_pair(tree_id, base::WrapUnique(tree_wrapper)));
    axtree_to_tree_wrapper_map_.insert(
        std::make_pair(tree_wrapper->tree(), tree_wrapper));
  } else {
    tree_wrapper = iter->second.get();
  }

  if (!tree_wrapper->OnAccessibilityEvent(params, is_active_profile)) {
    LOG(ERROR) << tree_wrapper->tree()->error();
    base::ListValue args;
    args.AppendInteger(tree_id);
    bindings_system_->DispatchEventInContext(
        "automationInternal.onAccessibilityTreeSerializationError", &args,
        nullptr, context());
    return;
  }
}

void AutomationInternalCustomBindings::OnAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  int tree_id = params.tree_id;
  auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return;
  AutomationAXTreeWrapper* tree_wrapper = iter->second.get();
  ui::AXNode* node = tree_wrapper->tree()->GetFromId(params.id);
  if (!node)
    return;
  node->SetLocation(params.new_location.offset_container_id,
                    params.new_location.bounds,
                    params.new_location.transform.get());
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

  auto iter = axtree_to_tree_wrapper_map_.find(tree);
  if (iter == axtree_to_tree_wrapper_map_.end())
    return;

  int tree_id = iter->second->tree_id();

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

    base::ListValue args;
    args.AppendInteger(observer.id);
    args.AppendInteger(tree_id);
    args.AppendInteger(node->id());
    args.AppendString(ToString(change_type));
    bindings_system_->DispatchEventInContext("automationInternal.onTreeChange",
                                             &args, nullptr, context());
  }
}

void AutomationInternalCustomBindings::SendAutomationEvent(
    const ExtensionMsg_AccessibilityEventParams& params,
    int target_id,
    api::automation::EventType event_type) {
  auto event_params = base::MakeUnique<base::DictionaryValue>();
  event_params->SetInteger("treeID", params.tree_id);
  event_params->SetInteger("targetID", target_id);
  event_params->SetString("eventType", api::automation::ToString(event_type));
  event_params->SetString("eventFrom", ToString(params.event_from));
  event_params->SetInteger("actionRequestID", params.action_request_id);
  event_params->SetInteger("mouseX", params.mouse_location.x());
  event_params->SetInteger("mouseY", params.mouse_location.y());
  base::ListValue args;
  args.Append(std::move(event_params));
  bindings_system_->DispatchEventInContext(
      "automationInternal.onAccessibilityEvent", &args, nullptr, context());
}

void AutomationInternalCustomBindings::SendChildTreeIDEvent(ui::AXTree* tree,
                                                            ui::AXNode* node) {
  auto iter = axtree_to_tree_wrapper_map_.find(tree);
  if (iter == axtree_to_tree_wrapper_map_.end())
    return;

  int tree_id = iter->second->tree_id();

  base::ListValue args;
  args.AppendInteger(tree_id);
  args.AppendInteger(node->id());
  bindings_system_->DispatchEventInContext("automationInternal.onChildTreeID",
                                           &args, nullptr, context());
}

void AutomationInternalCustomBindings::SendNodesRemovedEvent(
    ui::AXTree* tree,
    const std::vector<int>& ids) {
  auto iter = axtree_to_tree_wrapper_map_.find(tree);
  if (iter == axtree_to_tree_wrapper_map_.end())
    return;

  int tree_id = iter->second->tree_id();

  base::ListValue args;
  args.AppendInteger(tree_id);
  {
    auto nodes = base::MakeUnique<base::ListValue>();
    for (auto id : ids)
      nodes->AppendInteger(id);
    args.Append(std::move(nodes));
  }

  bindings_system_->DispatchEventInContext("automationInternal.onNodesRemoved",
                                           &args, nullptr, context());
}

}  // namespace extensions
