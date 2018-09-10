// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/i18n/string_search.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/extensions/api/automation_api_constants.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "ipc/message_filter.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_table_info.h"
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

ui::AXNode* GetContainingTable(ui::AXNode* node) {
  while (node && !ui::IsTableLikeRole(node->data().role))
    node = node->parent();

  if (ui::IsTableLikeRole(node->data().role))
    return node;

  return nullptr;
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

    int tree_id =
        args[0]
            ->Int32Value(automation_bindings_->context()->v8_context())
            .FromMaybe(0);
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
typedef std::function<void(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           AutomationAXTreeWrapper* tree_wrapper,
                           ui::AXNode* node)>
    NodeIDFunction;

class NodeIDWrapper : public base::RefCountedThreadSafe<NodeIDWrapper> {
 public:
  NodeIDWrapper(AutomationInternalCustomBindings* automation_bindings,
                NodeIDFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber())
      ThrowInvalidArgumentsException(automation_bindings_);

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    int tree_id = args[0]->Int32Value(context).FromMaybe(0);
    int node_id = args[1]->Int32Value(context).FromMaybe(0);

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
                                            ui::AXTree* tree,
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

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    int tree_id = args[0]->Int32Value(context).FromMaybe(0);
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    std::string attribute = *v8::String::Utf8Value(isolate, args[2]);

    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper->tree(), node,
              attribute);
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

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    int tree_id = args[0]->Int32Value(context).FromMaybe(0);
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    int start = args[2]->Int32Value(context).FromMaybe(0);
    int end = args[3]->Int32Value(context).FromMaybe(0);

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

typedef std::function<void(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           AutomationAXTreeWrapper* tree_wrapper,
                           ui::AXNode* node,
                           const std::string& strVal,
                           bool boolVal)>
    NodeIDPlusStringBoolFunction;

class NodeIDPlusStringBoolWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusStringBoolWrapper> {
 public:
  NodeIDPlusStringBoolWrapper(
      AutomationInternalCustomBindings* automation_bindings,
      NodeIDPlusStringBoolFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 4 || !args[0]->IsNumber() || !args[1]->IsNumber() ||
        !args[2]->IsString() || !args[3]->IsBoolean()) {
      ThrowInvalidArgumentsException(automation_bindings_);
    }

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    int tree_id = args[0]->Int32Value(context).FromMaybe(0);
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    std::string str_val = *v8::String::Utf8Value(isolate, args[2]);
    bool bool_val = args[3].As<v8::Boolean>()->Value();

    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node, str_val,
              bool_val);
  }

 private:
  virtual ~NodeIDPlusStringBoolWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusStringBoolWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusStringBoolFunction function_;
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
}

AutomationInternalCustomBindings::~AutomationInternalCustomBindings() {}

void AutomationInternalCustomBindings::AddRoutes() {
// It's safe to use base::Unretained(this) here because these bindings
// will only be called on a valid AutomationInternalCustomBindings instance
// and none of the functions have any side effects.
#define ROUTE_FUNCTION(FN)                                               \
  RouteHandlerFunction(#FN, "automation",                                \
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
            ui::ToString(tree_wrapper->tree()->data().sel_anchor_affinity)));
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
            ui::ToString(tree_wrapper->tree()->data().sel_focus_affinity)));
      });

  // Bindings that take a Tree ID and Node ID and return a property of the node.

  RouteNodeIDFunction(
      "GetParentID",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ui::AXNode* parent = GetParent(node, &tree_wrapper);
        if (parent) {
          gin::DataObjectBuilder response(isolate);
          response.Set("treeId", tree_wrapper->tree_id());
          response.Set("nodeId", parent->id());
          result.Set(response.Build());
        }
      });
  RouteNodeIDFunction(
      "GetChildCount",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        int child_count;
        if (GetRootOfChildTree(&node, &tree_wrapper))
          child_count = 1;
        else
          child_count = node->child_count();

        result.Set(v8::Integer::New(isolate, child_count));
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
        auto role = node->data().role;
        auto mapped_role = role;

        // These roles are remapped for simplicity in handling them in AT.
        switch (role) {
          case ax::mojom::Role::kLayoutTable:
            mapped_role = ax::mojom::Role::kTable;
            break;
          case ax::mojom::Role::kLayoutTableCell:
            mapped_role = ax::mojom::Role::kCell;
            break;
          case ax::mojom::Role::kLayoutTableColumn:
            mapped_role = ax::mojom::Role::kColumn;
            break;
          case ax::mojom::Role::kLayoutTableRow:
            mapped_role = ax::mojom::Role::kRow;
            break;
          default:
            break;
        }

        std::string role_name = ui::ToString(mapped_role);
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
  RouteNodeIDFunction(
      "GetChildIDs",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int> child_ids;
        if (GetRootOfChildTree(&node, &tree_wrapper)) {
          child_ids.push_back(node->id());
        } else {
          const std::vector<ui::AXNode*>& children = node->children();
          for (size_t i = 0; i < children.size(); ++i)
            child_ids.push_back(children[i]->id());
        }

        gin::DataObjectBuilder response(isolate);
        response.Set("treeId", tree_wrapper->tree_id());
        response.Set("nodeIds", child_ids);
        result.Set(response.Build());
      });

  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusRangeFunction(
      "GetBoundsForRange",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node, int start,
         int end) {
        if (node->data().role != ax::mojom::Role::kInlineTextBox) {
          gfx::Rect global_bounds = ComputeGlobalNodeBounds(tree_wrapper, node);
          result.Set(RectToV8Object(isolate, global_bounds));
        }

        // Use character offsets to compute the local bounds of this subrange.
        gfx::RectF local_bounds(0, 0, node->data().location.width(),
                                node->data().location.height());
        std::string name =
            node->data().GetStringAttribute(ax::mojom::StringAttribute::kName);
        std::vector<int> character_offsets = node->data().GetIntListAttribute(
            ax::mojom::IntListAttribute::kCharacterOffsets);
        int len =
            static_cast<int>(std::min(name.size(), character_offsets.size()));
        if (start >= 0 && start <= end && end <= len) {
          int start_offset = start > 0 ? character_offsets[start - 1] : 0;
          int end_offset = end > 0 ? character_offsets[end - 1] : 0;

          switch (node->data().GetTextDirection()) {
            case ax::mojom::TextDirection::kLtr:
            default:
              local_bounds.set_x(local_bounds.x() + start_offset);
              local_bounds.set_width(end_offset - start_offset);
              break;
            case ax::mojom::TextDirection::kRtl:
              local_bounds.set_x(local_bounds.x() + local_bounds.width() -
                                 end_offset);
              local_bounds.set_width(end_offset - start_offset);
              break;
            case ax::mojom::TextDirection::kTtb:
              local_bounds.set_y(local_bounds.y() + start_offset);
              local_bounds.set_height(end_offset - start_offset);
              break;
            case ax::mojom::TextDirection::kBtt:
              local_bounds.set_y(local_bounds.y() + local_bounds.height() -
                                 end_offset);
              local_bounds.set_height(end_offset - start_offset);
              break;
          }
        }

        // Convert from local to global coordinates second, after subsetting,
        // because the local to global conversion might involve matrix
        // transformations.
        gfx::Rect global_bounds = ComputeGlobalNodeBounds(
            tree_wrapper, node, local_bounds, nullptr, true /* clip_bounds */);
        result.Set(RectToV8Object(isolate, global_bounds));
      });

  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusAttributeFunction(
      "GetStringAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::StringAttribute attribute =
            ui::ParseStringAttribute(attribute_name.c_str());
        std::string attr_value;
        if (attribute == ax::mojom::StringAttribute::kFontFamily ||
            attribute == ax::mojom::StringAttribute::kLanguage) {
          attr_value = node->GetInheritedStringAttribute(attribute);
        } else if (!node->data().GetStringAttribute(attribute, &attr_value)) {
          return;
        }

        result.Set(v8::String::NewFromUtf8(isolate, attr_value.c_str()));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetBoolAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::BoolAttribute attribute =
            ui::ParseBoolAttribute(attribute_name.c_str());
        bool attr_value;
        if (!node->data().GetBoolAttribute(attribute, &attr_value))
          return;

        result.Set(v8::Boolean::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::IntAttribute attribute =
            ui::ParseIntAttribute(attribute_name.c_str());
        int attr_value;
        if (!node->data().GetIntAttribute(attribute, &attr_value))
          return;

        result.Set(v8::Integer::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttributeReverseRelations",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::IntAttribute attribute =
            ui::ParseIntAttribute(attribute_name.c_str());
        std::set<int32_t> ids =
            tree->GetReverseRelations(attribute, node->id());
        v8::Local<v8::Array> array_result(v8::Array::New(isolate, ids.size()));
        size_t count = 0;
        for (int32_t id : ids) {
          array_result->Set(static_cast<uint32_t>(count++),
                            v8::Integer::New(isolate, id));
        }
        result.Set(array_result);
      });
  RouteNodeIDPlusAttributeFunction(
      "GetFloatAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::FloatAttribute attribute =
            ui::ParseFloatAttribute(attribute_name.c_str());
        float attr_value;

        if (!node->data().GetFloatAttribute(attribute, &attr_value))
          return;

        double intpart, fracpart;
        fracpart = modf(attr_value, &intpart);
        double value_precision_2 =
            intpart + std::round(fracpart * 100) / 100.0f;
        result.Set(v8::Number::New(isolate, value_precision_2));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntListAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::IntListAttribute attribute =
            ui::ParseIntListAttribute(attribute_name.c_str());
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
      "GetIntListAttributeReverseRelations",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::IntListAttribute attribute =
            ui::ParseIntListAttribute(attribute_name.c_str());
        std::set<int32_t> ids =
            tree->GetReverseRelations(attribute, node->id());
        v8::Local<v8::Array> array_result(v8::Array::New(isolate, ids.size()));
        size_t count = 0;
        for (int32_t id : ids) {
          array_result->Set(static_cast<uint32_t>(count++),
                            v8::Integer::New(isolate, id));
        }
        result.Set(array_result);
      });
  RouteNodeIDPlusAttributeFunction(
      "GetHtmlAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        std::string attr_value;
        if (!node->data().GetHtmlAttribute(attribute_name.c_str(), &attr_value))
          return;

        result.Set(v8::String::NewFromUtf8(isolate, attr_value.c_str()));
      });
  RouteNodeIDFunction(
      "GetNameFrom",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::NameFrom name_from = static_cast<ax::mojom::NameFrom>(
            node->data().GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
        std::string name_from_str = ui::ToString(name_from);
        result.Set(v8::String::NewFromUtf8(isolate, name_from_str.c_str()));
      });
  RouteNodeIDFunction("GetSubscript", [](v8::Isolate* isolate,
                                         v8::ReturnValue<v8::Value> result,
                                         AutomationAXTreeWrapper* tree_wrapper,
                                         ui::AXNode* node) {
    bool value =
        node->data().GetIntAttribute(ax::mojom::IntAttribute::kTextPosition) ==
        static_cast<int32_t>(ax::mojom::TextPosition::kSubscript);
    result.Set(v8::Boolean::New(isolate, value));
  });
  RouteNodeIDFunction(
      "GetSuperscript",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            node->data().GetIntAttribute(
                ax::mojom::IntAttribute::kTextPosition) ==
            static_cast<int32_t>(ax::mojom::TextPosition::kSuperscript);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetBold", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                    AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            (node->data().GetIntAttribute(ax::mojom::IntAttribute::kTextStyle) &
             static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleBold)) != 0;
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetItalic", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            (node->data().GetIntAttribute(ax::mojom::IntAttribute::kTextStyle) &
             static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleItalic)) != 0;
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction("GetUnderline", [](v8::Isolate* isolate,
                                         v8::ReturnValue<v8::Value> result,
                                         AutomationAXTreeWrapper* tree_wrapper,
                                         ui::AXNode* node) {
    bool value =
        (node->data().GetIntAttribute(ax::mojom::IntAttribute::kTextStyle) &
         static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleUnderline)) != 0;
    result.Set(v8::Boolean::New(isolate, value));
  });
  RouteNodeIDFunction(
      "GetLineThrough",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            (node->data().GetIntAttribute(ax::mojom::IntAttribute::kTextStyle) &
             static_cast<int32_t>(
                 ax::mojom::TextStyle::kTextStyleLineThrough)) != 0;
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetCustomActions",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::vector<int32_t>& custom_action_ids =
            node->data().GetIntListAttribute(
                ax::mojom::IntListAttribute::kCustomActionIds);
        if (custom_action_ids.empty()) {
          result.SetUndefined();
          return;
        }

        const std::vector<std::string>& custom_action_descriptions =
            node->data().GetStringListAttribute(
                ax::mojom::StringListAttribute::kCustomActionDescriptions);
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
  RouteNodeIDFunction(
      "GetStandardActions",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<std::string> standard_actions;
        for (uint32_t action = static_cast<uint32_t>(ax::mojom::Action::kNone);
             action <= static_cast<uint32_t>(ax::mojom::Action::kMaxValue);
             ++action) {
          if (node->data().HasAction(static_cast<ax::mojom::Action>(action)))
            standard_actions.push_back(
                ToString(static_cast<api::automation::ActionType>(action)));
        }

        // The doDefault action is implied by having a default action verb.
        int default_action_verb =
            static_cast<int>(ax::mojom::DefaultActionVerb::kNone);
        if (node->data().GetIntAttribute(
                ax::mojom::IntAttribute::kDefaultActionVerb,
                &default_action_verb) &&
            default_action_verb !=
                static_cast<int>(ax::mojom::DefaultActionVerb::kNone)) {
          standard_actions.push_back(
              ToString(static_cast<api::automation::ActionType>(
                  ax::mojom::Action::kDoDefault)));
        }

        auto actions_result = v8::Array::New(isolate, standard_actions.size());
        for (size_t i = 0; i < standard_actions.size(); i++) {
          const v8::Maybe<bool>& did_set_value = actions_result->Set(
              isolate->GetCurrentContext(), i,
              v8::String::NewFromUtf8(isolate, standard_actions[i].c_str()));

          bool did_set_value_result = false;
          if (!did_set_value.To(&did_set_value_result) || !did_set_value_result)
            return;
        }
        result.Set(actions_result);
      });
  RouteNodeIDFunction(
      "GetChecked",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const ax::mojom::CheckedState checked_state =
            static_cast<ax::mojom::CheckedState>(node->data().GetIntAttribute(
                ax::mojom::IntAttribute::kCheckedState));
        if (checked_state != ax::mojom::CheckedState::kNone) {
          const std::string checked_str = ui::ToString(checked_state);
          result.Set(v8::String::NewFromUtf8(isolate, checked_str.c_str()));
        }
      });
  RouteNodeIDFunction(
      "GetRestriction",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const ax::mojom::Restriction restriction =
            node->data().GetRestriction();
        if (restriction != ax::mojom::Restriction::kNone) {
          const std::string restriction_str = ui::ToString(restriction);
          result.Set(v8::String::NewFromUtf8(isolate, restriction_str.c_str()));
        }
      });
  RouteNodeIDFunction(
      "GetDefaultActionVerb",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::DefaultActionVerb default_action_verb =
            static_cast<ax::mojom::DefaultActionVerb>(
                node->data().GetIntAttribute(
                    ax::mojom::IntAttribute::kDefaultActionVerb));
        std::string default_action_verb_str = ui::ToString(default_action_verb);
        result.Set(
            v8::String::NewFromUtf8(isolate, default_action_verb_str.c_str()));
      });
  RouteNodeIDPlusStringBoolFunction(
      "GetNextTextMatch",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node,
             const std::string& search_str, bool backward) {
        base::string16 search_str_16 = base::UTF8ToUTF16(search_str);
        auto func =
            backward ? &AutomationInternalCustomBindings::GetPreviousInTreeOrder
                     : &AutomationInternalCustomBindings::GetNextInTreeOrder;
        std::function<ui::AXNode*(ui::AXNode*, AutomationAXTreeWrapper**)> next(
            std::bind(func, this, std::placeholders::_1,
                      std::placeholders::_2));

        AutomationAXTreeWrapper** target_tree_wrapper = &tree_wrapper;
        while (true) {
          node = next(node, target_tree_wrapper);

          // We explicitly disallow searches in the desktop tree.
          if ((*target_tree_wrapper)->tree_id() ==
              api::automation::kDesktopTreeID)
            return;

          if (!node)
            return;

          base::string16 name;
          if (!node->GetString16Attribute(ax::mojom::StringAttribute::kName,
                                          &name))
            continue;

          if (base::i18n::StringSearchIgnoringCaseAndAccents(
                  search_str_16, name, nullptr, nullptr)) {
            gin::DataObjectBuilder response(isolate);
            response.Set("treeId", (*target_tree_wrapper)->tree_id());
            response.Set("nodeId", node->id());
            result.Set(response.Build());
            return;
          }
        }
      });
  RouteNodeIDFunction(
      "GetTableCellColumnHeaders",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ui::AXTree* tree = tree_wrapper->tree();

        int32_t column_index;
        if (!node->GetIntAttribute(
                ax::mojom::IntAttribute::kTableCellColumnIndex, &column_index))
          return;

        ui::AXNode* table = GetContainingTable(node);
        if (!table)
          return;

        ui::AXTableInfo* table_info = tree->GetTableInfo(table);
        if (column_index < 0 || column_index >= table_info->col_count)
          return;

        std::vector<int32_t>& col_headers =
            table_info->col_headers[column_index];
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, col_headers.size()));
        for (size_t i = 0; i < col_headers.size(); ++i)
          array_result->Set(static_cast<uint32_t>(i),
                            v8::Integer::New(isolate, col_headers[i]));
        result.Set(array_result);
      });
  RouteNodeIDFunction(
      "GetTableCellRowHeaders",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ui::AXTree* tree = tree_wrapper->tree();
        int32_t row_index;
        if (!node->GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                                   &row_index))
          return;

        ui::AXNode* table = GetContainingTable(node);
        if (!table)
          return;

        ui::AXTableInfo* table_info = tree->GetTableInfo(table);
        if (row_index < 0 || row_index >= table_info->row_count)
          return;

        std::vector<int32_t>& row_headers = table_info->row_headers[row_index];
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, row_headers.size()));
        for (size_t i = 0; i < row_headers.size(); ++i)
          array_result->Set(static_cast<uint32_t>(i),
                            v8::Integer::New(isolate, row_headers[i]));
        result.Set(array_result);
      });
}

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
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityEventBundle,
                        OnAccessibilityEvents)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityLocationChange,
                        OnAccessibilityLocationChange)
  IPC_END_MESSAGE_MAP()
}  // clang-format on

AutomationAXTreeWrapper* AutomationInternalCustomBindings::
    GetAutomationAXTreeWrapperFromTreeID(int tree_id) const {
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
  for (int32_t i = static_cast<int32_t>(ax::mojom::NameFrom::kNone);
       i <= static_cast<int32_t>(ax::mojom::NameFrom::kMaxValue); ++i) {
    name_from_type.Set(
        i,
        base::StringPiece(ui::ToString(static_cast<ax::mojom::NameFrom>(i))));
  }

  gin::DataObjectBuilder restriction(isolate);
  for (int32_t i = static_cast<int32_t>(ax::mojom::Restriction::kNone);
       i <= static_cast<int32_t>(ax::mojom::Restriction::kMaxValue); ++i) {
    restriction.Set(i, base::StringPiece(ui::ToString(
                           static_cast<ax::mojom::Restriction>(i))));
  }

  gin::DataObjectBuilder description_from_type(isolate);
  for (int32_t i = static_cast<int32_t>(ax::mojom::DescriptionFrom::kNone);
       i <= static_cast<int32_t>(ax::mojom::DescriptionFrom::kMaxValue); ++i) {
    description_from_type.Set(
        i, base::StringPiece(
               ui::ToString(static_cast<ax::mojom::DescriptionFrom>(i))));
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

  int tree_id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);
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
  observer.id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);
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

  int observer_id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);

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
  while (focus->data().HasIntAttribute(ax::mojom::IntAttribute::kChildTreeId)) {
    // Try to keep following focus recursively, by letting |tree_id| be the
    // new subtree to search in, while keeping |focus_tree_id| set to the tree
    // where we know we found a focused node.
    int child_tree_id =
        focus->data().GetIntAttribute(ax::mojom::IntAttribute::kChildTreeId);

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

  int tree_id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);
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

  int tree_id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);
  int node_id = args[1]->Int32Value(context()->v8_context()).FromMaybe(0);

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

  int tree_id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);
  int node_id = args[1]->Int32Value(context()->v8_context()).FromMaybe(0);

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
      state.Set(ui::ToString(static_cast<ax::mojom::State>(state_pos)), true);
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
    state.Set(api::automation::ToString(api::automation::STATE_TYPE_FOCUSED),
              true);

  bool offscreen = false;
  ComputeGlobalNodeBounds(tree_wrapper, node, gfx::RectF(), &offscreen);
  if (offscreen)
    state.Set(api::automation::ToString(api::automation::STATE_TYPE_OFFSCREEN),
              true);

  args.GetReturnValue().Set(state.Build());
}

void AutomationInternalCustomBindings::UpdateOverallTreeChangeObserverFilter() {
  tree_change_observer_overall_filter_ = 0;
  for (const auto& observer : tree_change_observers_)
    tree_change_observer_overall_filter_ |= 1 << observer.filter;
}

ui::AXNode* AutomationInternalCustomBindings::GetParent(
    ui::AXNode* node,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
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

  std::set<int32_t> host_node_ids =
      parent_tree_wrapper->tree()->GetNodeIdsForChildTreeId(
          (*in_out_tree_wrapper)->tree_id());

#if !defined(NDEBUG)
  if (host_node_ids.size() > 1)
    DLOG(WARNING) << "Multiple nodes claim the same child tree id.";
#endif

  for (int32_t host_node_id : host_node_ids) {
    ui::AXNode* host_node =
        parent_tree_wrapper->tree()->GetFromId(host_node_id);
    if (host_node) {
      DCHECK_EQ(
          (*in_out_tree_wrapper)->tree_id(),
          host_node->GetIntAttribute(ax::mojom::IntAttribute::kChildTreeId));
      *in_out_tree_wrapper = parent_tree_wrapper;
      return host_node;
    }
  }

  return nullptr;
}

bool AutomationInternalCustomBindings::GetRootOfChildTree(
    ui::AXNode** in_out_node,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  int child_tree_id;
  if (!(*in_out_node)
           ->GetIntAttribute(ax::mojom::IntAttribute::kChildTreeId,
                             &child_tree_id))
    return false;

  AutomationAXTreeWrapper* child_tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(child_tree_id);
  if (!child_tree_wrapper)
    return false;

  *in_out_tree_wrapper = child_tree_wrapper;
  *in_out_node = child_tree_wrapper->tree()->root();
  return true;
}

ui::AXNode* AutomationInternalCustomBindings::GetNextInTreeOrder(
    ui::AXNode* start,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  ui::AXNode* walker = start;
  if (walker->child_count() > 0)
    return walker->ChildAtIndex(0);

  // We also have to check child tree id.
  if (GetRootOfChildTree(&walker, in_out_tree_wrapper))
    return walker;

  // Find the next branch forward.
  ui::AXNode* parent;
  while ((parent = GetParent(walker, in_out_tree_wrapper))) {
    if ((walker->index_in_parent() + 1) < parent->child_count())
      return parent->ChildAtIndex(walker->index_in_parent() + 1);

    walker = parent;
  }

  return nullptr;
}

ui::AXNode* AutomationInternalCustomBindings::GetPreviousInTreeOrder(
    ui::AXNode* start,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  ui::AXNode* walker = start;

  ui::AXNode* parent = GetParent(start, in_out_tree_wrapper);

  // No more nodes.
  if (!parent)
    return nullptr;

  // No previous sibling; parent is previous.
  if (walker->index_in_parent() == 0)
    return parent;

  walker = parent->ChildAtIndex(walker->index_in_parent() - 1);

  // Walks to deepest last child.
  while (true) {
    if (walker->child_count() > 0) {
      walker = walker->ChildAtIndex(walker->child_count() - 1);
    } else if (!GetRootOfChildTree(&walker, in_out_tree_wrapper)) {
      break;
    }
  }
  return walker;
}

float AutomationInternalCustomBindings::GetDeviceScaleFactor() const {
  return context()->GetRenderFrame()->GetRenderView()->GetDeviceScaleFactor();
}

void AutomationInternalCustomBindings::RouteTreeIDFunction(
    const std::string& name,
    TreeIDFunction callback) {
  scoped_refptr<TreeIDWrapper> wrapper = new TreeIDWrapper(this, callback);
  RouteHandlerFunction(name, base::Bind(&TreeIDWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDFunction(
    const std::string& name,
    NodeIDFunction callback) {
  scoped_refptr<NodeIDWrapper> wrapper = new NodeIDWrapper(this, callback);
  RouteHandlerFunction(name, base::BindRepeating(&NodeIDWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusAttributeFunction(
    const std::string& name,
    NodeIDPlusAttributeFunction callback) {
  scoped_refptr<NodeIDPlusAttributeWrapper> wrapper =
      new NodeIDPlusAttributeWrapper(this, callback);
  RouteHandlerFunction(name,
                       base::Bind(&NodeIDPlusAttributeWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusRangeFunction(
    const std::string& name,
    NodeIDPlusRangeFunction callback) {
  scoped_refptr<NodeIDPlusRangeWrapper> wrapper =
      new NodeIDPlusRangeWrapper(this, callback);
  RouteHandlerFunction(name, base::Bind(&NodeIDPlusRangeWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusStringBoolFunction(
    const std::string& name,
    NodeIDPlusStringBoolFunction callback) {
  scoped_refptr<NodeIDPlusStringBoolWrapper> wrapper =
      new NodeIDPlusStringBoolWrapper(this, callback);
  RouteHandlerFunction(
      name, base::BindRepeating(&NodeIDPlusStringBoolWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::GetChildIDAtIndex(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 3 || !args[2]->IsNumber()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  int tree_id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);
  int node_id = args[1]->Int32Value(context()->v8_context()).FromMaybe(0);

  const auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return;

  AutomationAXTreeWrapper* tree_wrapper = iter->second.get();
  ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
  if (!node)
    return;

  int index = args[2]->Int32Value(context()->v8_context()).FromMaybe(0);
  int child_id;

  // Check for a child tree, which is guaranteed to always be the only child.
  if (index == 0 && GetRootOfChildTree(&node, &tree_wrapper))
    child_id = node->id();
  else if (index < 0 || index >= node->child_count())
    return;
  else
    child_id = node->children()[index]->id();

  gin::DataObjectBuilder response(GetIsolate());
  response.Set("treeId", tree_wrapper->tree_id());
  response.Set("nodeId", child_id);
  args.GetReturnValue().Set(response.Build());
}

//
// Handle accessibility events from the browser process.
//

void AutomationInternalCustomBindings::OnAccessibilityEvents(
    const ExtensionMsg_AccessibilityEventBundleParams& event_bundle,
    bool is_active_profile) {
  is_active_profile_ = is_active_profile;
  int tree_id = event_bundle.tree_id;
  AutomationAXTreeWrapper* tree_wrapper;
  auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end()) {
    tree_wrapper = new AutomationAXTreeWrapper(tree_id, this);
    tree_id_to_tree_wrapper_map_.insert(
        std::make_pair(tree_id, base::WrapUnique(tree_wrapper)));
    axtree_to_tree_wrapper_map_.insert(
        std::make_pair(tree_wrapper->tree(), tree_wrapper));
  } else {
    tree_wrapper = iter->second.get();
  }

  if (!tree_wrapper->OnAccessibilityEvents(event_bundle, is_active_profile)) {
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

bool AutomationInternalCustomBindings::SendTreeChangeEvent(
    api::automation::TreeChangeType change_type,
    ui::AXTree* tree,
    ui::AXNode* node) {
  // Don't send tree change events when it's not the active profile.
  if (!is_active_profile_)
    return false;

  // Notify custom bindings when there's an unloaded tree; js will enable the
  // renderer and wait for it to load.
  int child_tree_id;
  if (node->data().GetIntAttribute(ax::mojom::IntAttribute::kChildTreeId,
                                   &child_tree_id)) {
    auto* tree_wrapper = GetAutomationAXTreeWrapperFromTreeID(child_tree_id);
    if (!tree_wrapper || !tree_wrapper->tree()->data().loaded)
      SendChildTreeIDEvent(child_tree_id);
  }

  bool has_filter = false;
  if (tree_change_observer_overall_filter_ &
      (1 <<
       api::automation::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES)) {
    if (node->data().HasStringAttribute(
            ax::mojom::StringAttribute::kContainerLiveStatus) ||
        node->data().role == ax::mojom::Role::kAlert ||
        change_type == api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND) {
      has_filter = true;
    }
  }

  if (tree_change_observer_overall_filter_ &
      (1 << api::automation::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES)) {
    if (node->data().HasIntListAttribute(
            ax::mojom::IntListAttribute::kMarkerTypes))
      has_filter = true;
  }

  if (tree_change_observer_overall_filter_ &
      (1 << api::automation::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES))
    has_filter = true;

  if (!has_filter)
    return false;

  auto iter = axtree_to_tree_wrapper_map_.find(tree);
  if (iter == axtree_to_tree_wrapper_map_.end())
    return false;

  int tree_id = iter->second->tree_id();
  bool did_send_event = false;
  for (const auto& observer : tree_change_observers_) {
    switch (observer.filter) {
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_NOTREECHANGES:
      default:
        continue;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES:
        if (!node->data().HasStringAttribute(
                ax::mojom::StringAttribute::kContainerLiveStatus) &&
            node->data().role != ax::mojom::Role::kAlert &&
            change_type != api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND) {
          continue;
        }
        break;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES:
        if (!node->data().HasIntListAttribute(
                ax::mojom::IntListAttribute::kMarkerTypes))
          continue;
        break;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES:
        break;
    }

    did_send_event = true;
    base::ListValue args;
    args.AppendInteger(observer.id);
    args.AppendInteger(tree_id);
    args.AppendInteger(node->id());
    args.AppendString(ToString(change_type));
    bindings_system_->DispatchEventInContext("automationInternal.onTreeChange",
                                             &args, nullptr, context());
  }

  return did_send_event;
}

void AutomationInternalCustomBindings::SendAutomationEvent(
    int tree_id,
    const gfx::Point& mouse_location,
    const ui::AXEvent& event,
    api::automation::EventType event_type) {
  auto event_params = std::make_unique<base::DictionaryValue>();
  event_params->SetInteger("treeID", tree_id);
  event_params->SetInteger("targetID", event.id);
  event_params->SetString("eventType", api::automation::ToString(event_type));
  event_params->SetString("eventFrom", ui::ToString(event.event_from));
  event_params->SetInteger("actionRequestID", event.action_request_id);
  event_params->SetInteger("mouseX", mouse_location.x());
  event_params->SetInteger("mouseY", mouse_location.y());
  base::ListValue args;
  args.Append(std::move(event_params));
  bindings_system_->DispatchEventInContext(
      "automationInternal.onAccessibilityEvent", &args, nullptr, context());
}

void AutomationInternalCustomBindings::SendChildTreeIDEvent(int child_tree_id) {
  base::ListValue args;
  args.AppendInteger(child_tree_id);
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
    auto nodes = std::make_unique<base::ListValue>();
    for (auto id : ids)
      nodes->AppendInteger(id);
    args.Append(std::move(nodes));
  }

  bindings_system_->DispatchEventInContext("automationInternal.onNodesRemoved",
                                           &args, nullptr, context());
}

}  // namespace extensions
