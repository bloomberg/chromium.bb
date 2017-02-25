// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include <string>

#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"

namespace {

ui::AXEvent ToAXEvent(arc::mojom::AccessibilityEventType arc_event_type) {
  switch (arc_event_type) {
    case arc::mojom::AccessibilityEventType::VIEW_FOCUSED:
    case arc::mojom::AccessibilityEventType::VIEW_ACCESSIBILITY_FOCUSED:
      return ui::AX_EVENT_FOCUS;
    case arc::mojom::AccessibilityEventType::VIEW_ACCESSIBILITY_FOCUS_CLEARED:
      return ui::AX_EVENT_BLUR;
    case arc::mojom::AccessibilityEventType::VIEW_CLICKED:
    case arc::mojom::AccessibilityEventType::VIEW_LONG_CLICKED:
      return ui::AX_EVENT_CLICKED;
    case arc::mojom::AccessibilityEventType::VIEW_TEXT_CHANGED:
      return ui::AX_EVENT_TEXT_CHANGED;
    case arc::mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED:
      return ui::AX_EVENT_TEXT_SELECTION_CHANGED;
    case arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED:
    case arc::mojom::AccessibilityEventType::NOTIFICATION_STATE_CHANGED:
    case arc::mojom::AccessibilityEventType::WINDOW_CONTENT_CHANGED:
    case arc::mojom::AccessibilityEventType::WINDOWS_CHANGED:
      return ui::AX_EVENT_LAYOUT_COMPLETE;
    case arc::mojom::AccessibilityEventType::VIEW_HOVER_ENTER:
      return ui::AX_EVENT_HOVER;
    case arc::mojom::AccessibilityEventType::ANNOUNCEMENT:
      return ui::AX_EVENT_ALERT;
    case arc::mojom::AccessibilityEventType::VIEW_SELECTED:
    case arc::mojom::AccessibilityEventType::VIEW_HOVER_EXIT:
    case arc::mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_START:
    case arc::mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_END:
    case arc::mojom::AccessibilityEventType::VIEW_SCROLLED:
    case arc::mojom::AccessibilityEventType::
        VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
    case arc::mojom::AccessibilityEventType::GESTURE_DETECTION_START:
    case arc::mojom::AccessibilityEventType::GESTURE_DETECTION_END:
    case arc::mojom::AccessibilityEventType::TOUCH_INTERACTION_START:
    case arc::mojom::AccessibilityEventType::TOUCH_INTERACTION_END:
    case arc::mojom::AccessibilityEventType::VIEW_CONTEXT_CLICKED:
    case arc::mojom::AccessibilityEventType::ASSIST_READING_CONTEXT:
      return ui::AX_EVENT_CHILDREN_CHANGED;
    default:
      return ui::AX_EVENT_CHILDREN_CHANGED;
  }
  return ui::AX_EVENT_CHILDREN_CHANGED;
}

const gfx::Rect GetBounds(arc::mojom::AccessibilityNodeInfoData* node) {
  exo::WMHelper* wmHelper = exo::WMHelper::GetInstance();
  aura::Window* focused_window = wmHelper->GetFocusedWindow();
  gfx::Rect bounds_in_screen = node->boundsInScreen;
  if (focused_window) {
    aura::Window* toplevel_window = focused_window->GetToplevelWindow();
    return gfx::ScaleToEnclosingRect(
        bounds_in_screen,
        1.0f / toplevel_window->layer()->device_scale_factor());
  }
  return bounds_in_screen;
}

bool GetStringProperty(arc::mojom::AccessibilityNodeInfoData* node,
                       arc::mojom::AccessibilityStringProperty prop,
                       std::string* out_value) {
  if (!node->stringProperties)
    return false;

  auto it = node->stringProperties->find(prop);
  if (it == node->stringProperties->end())
    return false;

  *out_value = it->second;
  return true;
}

}  // namespace

namespace arc {

AXTreeSourceArc::AXTreeSourceArc(int32_t id)
    : tree_id_(id),
      current_tree_serializer_(new AXTreeArcSerializer(this)),
      root_id_(-1),
      focused_node_id_(-1) {}

AXTreeSourceArc::~AXTreeSourceArc() {
  Reset();
}

void AXTreeSourceArc::NotifyAccessibilityEvent(
    mojom::AccessibilityEventData* event_data) {
  tree_map_.clear();
  parent_map_.clear();
  root_id_ = -1;
  focused_node_id_ = -1;
  for (size_t i = 0; i < event_data->nodeData.size(); ++i) {
    if (!event_data->nodeData[i]->intListProperties)
      continue;
    auto it = event_data->nodeData[i]->intListProperties->find(
        arc::mojom::AccessibilityIntListProperty::CHILD_NODE_IDS);
    if (it != event_data->nodeData[i]->intListProperties->end()) {
      for (size_t j = 0; j < it->second.size(); ++j)
        parent_map_[it->second[j]] = event_data->nodeData[i]->id;
    }
  }

  for (size_t i = 0; i < event_data->nodeData.size(); ++i) {
    int32_t id = event_data->nodeData[i]->id;
    tree_map_[id] = event_data->nodeData[i].get();
    if (parent_map_.find(id) == parent_map_.end()) {
      CHECK_EQ(-1, root_id_) << "Duplicated root";
      root_id_ = id;
    }
  }

  ExtensionMsg_AccessibilityEventParams params;
  params.event_type = ToAXEvent(event_data->eventType);
  if (params.event_type == ui::AX_EVENT_FOCUS)
    focused_node_id_ = params.id;
  params.tree_id = tree_id_;
  params.id = event_data->sourceId;

  current_tree_serializer_->SerializeChanges(GetFromId(event_data->sourceId),
                                             &params.update);

  extensions::AutomationEventRouter* router =
      extensions::AutomationEventRouter::GetInstance();
  router->DispatchAccessibilityEvent(params);
}

bool AXTreeSourceArc::GetTreeData(ui::AXTreeData* data) const {
  data->tree_id = tree_id_;
  if (focused_node_id_ >= 0)
    data->focus_id = focused_node_id_;
  return true;
}

mojom::AccessibilityNodeInfoData* AXTreeSourceArc::GetRoot() const {
  mojom::AccessibilityNodeInfoData* root = GetFromId(root_id_);
  return root;
}

mojom::AccessibilityNodeInfoData* AXTreeSourceArc::GetFromId(int32_t id) const {
  auto it = tree_map_.find(id);
  if (it == tree_map_.end())
    return nullptr;
  return it->second;
}

int32_t AXTreeSourceArc::GetId(mojom::AccessibilityNodeInfoData* node) const {
  if (!node)
    return -1;
  return node->id;
}

void AXTreeSourceArc::GetChildren(
    mojom::AccessibilityNodeInfoData* node,
    std::vector<mojom::AccessibilityNodeInfoData*>* out_children) const {
  if (!node || !node->intListProperties)
    return;

  auto it = node->intListProperties->find(
      arc::mojom::AccessibilityIntListProperty::CHILD_NODE_IDS);
  if (it == node->intListProperties->end())
    return;

  for (size_t i = 0; i < it->second.size(); ++i) {
    out_children->push_back(GetFromId(it->second[i]));
  }
}

mojom::AccessibilityNodeInfoData* AXTreeSourceArc::GetParent(
    mojom::AccessibilityNodeInfoData* node) const {
  if (!node)
    return nullptr;
  auto it = parent_map_.find(node->id);
  if (it != parent_map_.end())
    return GetFromId(it->second);
  return nullptr;
}

bool AXTreeSourceArc::IsValid(mojom::AccessibilityNodeInfoData* node) const {
  return node;
}

bool AXTreeSourceArc::IsEqual(mojom::AccessibilityNodeInfoData* node1,
                              mojom::AccessibilityNodeInfoData* node2) const {
  if (!node1 || !node2)
    return false;
  return node1->id == node2->id;
}

mojom::AccessibilityNodeInfoData* AXTreeSourceArc::GetNull() const {
  return nullptr;
}

void AXTreeSourceArc::SerializeNode(mojom::AccessibilityNodeInfoData* node,
                                    ui::AXNodeData* out_data) const {
  if (!node)
    return;
  out_data->id = node->id;
  out_data->state = 0;

  using AXStringProperty = arc::mojom::AccessibilityStringProperty;
  std::string text;
  if (GetStringProperty(node, AXStringProperty::TEXT, &text))
    out_data->SetName(text);
  else if (GetStringProperty(node, AXStringProperty::CONTENT_DESCRIPTION,
                             &text))
    out_data->SetName(text);

  int32_t id = node->id;
  if (id == root_id_)
    out_data->role = ui::AX_ROLE_ROOT_WEB_AREA;
  else if (!text.empty())
    out_data->role = ui::AX_ROLE_STATIC_TEXT;
  else
    out_data->role = ui::AX_ROLE_DIV;

  const gfx::Rect bounds_in_screen = GetBounds(node);
  out_data->location.SetRect(bounds_in_screen.x(), bounds_in_screen.y(),
                             bounds_in_screen.width(),
                             bounds_in_screen.height());
}

void AXTreeSourceArc::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  current_tree_serializer_.reset(new AXTreeArcSerializer(this));
  root_id_ = -1;
  focused_node_id_ = -1;
  extensions::AutomationEventRouter* router =
      extensions::AutomationEventRouter::GetInstance();
  router->DispatchTreeDestroyedEvent(tree_id_, nullptr);
}

}  // namespace arc
