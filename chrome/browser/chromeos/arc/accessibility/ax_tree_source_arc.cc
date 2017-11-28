// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include <string>

#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "components/exo/wm_helper.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/aura/window.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

ui::AXEvent ToAXEvent(arc::mojom::AccessibilityEventType arc_event_type) {
  switch (arc_event_type) {
    case arc::mojom::AccessibilityEventType::VIEW_FOCUSED:
    case arc::mojom::AccessibilityEventType::VIEW_ACCESSIBILITY_FOCUSED:
      return ui::AX_EVENT_FOCUS;
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
    case arc::mojom::AccessibilityEventType::VIEW_SCROLLED:
      return ui::AX_EVENT_SCROLL_POSITION_CHANGED;
    case arc::mojom::AccessibilityEventType::VIEW_SELECTED:
    case arc::mojom::AccessibilityEventType::VIEW_HOVER_EXIT:
    case arc::mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_START:
    case arc::mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_END:
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

bool GetBooleanProperty(arc::mojom::AccessibilityNodeInfoData* node,
                        arc::mojom::AccessibilityBooleanProperty prop) {
  if (!node->boolean_properties)
    return false;

  auto it = node->boolean_properties->find(prop);
  if (it == node->boolean_properties->end())
    return false;

  return it->second;
}

bool GetIntProperty(arc::mojom::AccessibilityNodeInfoData* node,
                    arc::mojom::AccessibilityIntProperty prop,
                    int32_t* out_value) {
  if (!node->int_properties)
    return false;

  auto it = node->int_properties->find(prop);
  if (it == node->int_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

bool GetStringProperty(arc::mojom::AccessibilityNodeInfoData* node,
                       arc::mojom::AccessibilityStringProperty prop,
                       std::string* out_value) {
  if (!node->string_properties)
    return false;

  auto it = node->string_properties->find(prop);
  if (it == node->string_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

bool GetIntListProperty(arc::mojom::AccessibilityNodeInfoData* node,
                        arc::mojom::AccessibilityIntListProperty prop,
                        std::vector<int32_t>* out_value) {
  if (!node->int_list_properties)
    return false;

  auto it = node->int_list_properties->find(prop);
  if (it == node->int_list_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

bool GetStringListProperty(arc::mojom::AccessibilityNodeInfoData* node,
                           arc::mojom::AccessibilityStringListProperty prop,
                           std::vector<std::string>* out_value) {
  if (!node->string_list_properties)
    return false;

  auto it = node->string_list_properties->find(prop);
  if (it == node->string_list_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

bool HasCoveringSpan(arc::mojom::AccessibilityNodeInfoData* data,
                     arc::mojom::AccessibilityStringProperty prop,
                     arc::mojom::SpanType span_type) {
  if (!data->spannable_string_properties)
    return false;

  std::string text;
  GetStringProperty(data, prop, &text);
  if (text.empty())
    return false;

  auto span_entries_it = data->spannable_string_properties->find(prop);
  if (span_entries_it == data->spannable_string_properties->end())
    return false;

  for (size_t i = 0; i < span_entries_it->second.size(); ++i) {
    if (span_entries_it->second[i]->span_type != span_type)
      continue;

    size_t span_size =
        span_entries_it->second[i]->end - span_entries_it->second[i]->start;
    if (span_size == text.size())
      return true;
  }
  return false;
}

void PopulateAXRole(arc::mojom::AccessibilityNodeInfoData* node,
                    ui::AXNodeData* out_data) {
  if (HasCoveringSpan(node, arc::mojom::AccessibilityStringProperty::TEXT,
                      arc::mojom::SpanType::URL) ||
      HasCoveringSpan(
          node, arc::mojom::AccessibilityStringProperty::CONTENT_DESCRIPTION,
          arc::mojom::SpanType::URL)) {
    out_data->role = ui::AX_ROLE_LINK;
    return;
  }

  arc::mojom::AccessibilityCollectionItemInfoData* collection_item_info =
      node->collection_item_info.get();
  if (collection_item_info) {
    if (collection_item_info->is_heading) {
      out_data->role = ui::AX_ROLE_HEADING;
    } else {
      out_data->role = ui::AX_ROLE_LIST_ITEM;
      out_data->AddIntAttribute(ui::AX_ATTR_POS_IN_SET,
                                collection_item_info->row_index);
    }
    return;
  }

  std::string chrome_role;
  if (GetStringProperty(node,
                        arc::mojom::AccessibilityStringProperty::CHROME_ROLE,
                        &chrome_role)) {
    ui::AXRole role_value = ui::ParseAXRole(chrome_role);
    if (role_value != ui::AX_ROLE_NONE) {
      out_data->role = role_value;
      return;
    }
  }

  std::string class_name;
  GetStringProperty(node, arc::mojom::AccessibilityStringProperty::CLASS_NAME,
                    &class_name);

#define MAP_ROLE(android_class_name, chrome_role) \
  if (class_name == android_class_name) {         \
    out_data->role = chrome_role;                 \
    return;                                       \
  }

  // These mappings were taken from accessibility utils (Android -> Chrome) and
  // BrowserAccessibilityAndroid. They do not completely match the above two
  // sources.
  MAP_ROLE(ui::kAXAbsListViewClassname, ui::AX_ROLE_LIST);
  MAP_ROLE(ui::kAXButtonClassname, ui::AX_ROLE_BUTTON);
  MAP_ROLE(ui::kAXCheckBoxClassname, ui::AX_ROLE_CHECK_BOX);
  MAP_ROLE(ui::kAXCheckedTextViewClassname, ui::AX_ROLE_STATIC_TEXT);
  MAP_ROLE(ui::kAXCompoundButtonClassname, ui::AX_ROLE_CHECK_BOX);
  MAP_ROLE(ui::kAXDialogClassname, ui::AX_ROLE_DIALOG);
  MAP_ROLE(ui::kAXEditTextClassname, ui::AX_ROLE_TEXT_FIELD);
  MAP_ROLE(ui::kAXGridViewClassname, ui::AX_ROLE_TABLE);
  MAP_ROLE(ui::kAXImageClassname, ui::AX_ROLE_IMAGE);
  if (GetBooleanProperty(node,
                         arc::mojom::AccessibilityBooleanProperty::CLICKABLE)) {
    MAP_ROLE(ui::kAXImageViewClassname, ui::AX_ROLE_BUTTON);
  } else {
    MAP_ROLE(ui::kAXImageViewClassname, ui::AX_ROLE_IMAGE);
  }
  MAP_ROLE(ui::kAXListViewClassname, ui::AX_ROLE_LIST);
  MAP_ROLE(ui::kAXMenuItemClassname, ui::AX_ROLE_MENU_ITEM);
  MAP_ROLE(ui::kAXPagerClassname, ui::AX_ROLE_GROUP);
  MAP_ROLE(ui::kAXProgressBarClassname, ui::AX_ROLE_PROGRESS_INDICATOR);
  MAP_ROLE(ui::kAXRadioButtonClassname, ui::AX_ROLE_RADIO_BUTTON);
  MAP_ROLE(ui::kAXSeekBarClassname, ui::AX_ROLE_SLIDER);
  MAP_ROLE(ui::kAXSpinnerClassname, ui::AX_ROLE_POP_UP_BUTTON);
  MAP_ROLE(ui::kAXSwitchClassname, ui::AX_ROLE_SWITCH);
  MAP_ROLE(ui::kAXTabWidgetClassname, ui::AX_ROLE_TAB_LIST);
  MAP_ROLE(ui::kAXToggleButtonClassname, ui::AX_ROLE_TOGGLE_BUTTON);
  MAP_ROLE(ui::kAXViewClassname, ui::AX_ROLE_GENERIC_CONTAINER);
  MAP_ROLE(ui::kAXViewGroupClassname, ui::AX_ROLE_GROUP);
  MAP_ROLE(ui::kAXWebViewClassname, ui::AX_ROLE_WEB_VIEW);

#undef MAP_ROLE

  std::string text;
  GetStringProperty(node, arc::mojom::AccessibilityStringProperty::TEXT, &text);
  if (!text.empty())
    out_data->role = ui::AX_ROLE_STATIC_TEXT;
  else
    out_data->role = ui::AX_ROLE_GENERIC_CONTAINER;
}

void PopulateAXState(arc::mojom::AccessibilityNodeInfoData* node,
                     ui::AXNodeData* out_data) {
#define MAP_STATE(android_boolean_property, chrome_state) \
  if (GetBooleanProperty(node, android_boolean_property)) \
    out_data->AddState(chrome_state);

  using AXBooleanProperty = arc::mojom::AccessibilityBooleanProperty;

  // These mappings were taken from accessibility utils (Android -> Chrome) and
  // BrowserAccessibilityAndroid. They do not completely match the above two
  // sources.
  MAP_STATE(AXBooleanProperty::EDITABLE, ui::AX_STATE_EDITABLE);
  MAP_STATE(AXBooleanProperty::FOCUSABLE, ui::AX_STATE_FOCUSABLE);
  MAP_STATE(AXBooleanProperty::MULTI_LINE, ui::AX_STATE_MULTILINE);
  MAP_STATE(AXBooleanProperty::PASSWORD, ui::AX_STATE_PROTECTED);
  MAP_STATE(AXBooleanProperty::SELECTED, ui::AX_STATE_SELECTED);

#undef MAP_STATE

  if (GetBooleanProperty(node, AXBooleanProperty::CHECKABLE)) {
    const bool is_checked =
        GetBooleanProperty(node, AXBooleanProperty::CHECKED);
    const ui::AXCheckedState checked_state =
        is_checked ? ui::AX_CHECKED_STATE_TRUE : ui::AX_CHECKED_STATE_FALSE;
    out_data->AddIntAttribute(ui::AX_ATTR_CHECKED_STATE, checked_state);
  }

  if (!GetBooleanProperty(node, AXBooleanProperty::ENABLED)) {
    out_data->AddIntAttribute(ui::AX_ATTR_RESTRICTION,
                              ui::AX_RESTRICTION_DISABLED);
  }

  if (!GetBooleanProperty(node, AXBooleanProperty::VISIBLE_TO_USER)) {
    out_data->AddState(ui::AX_STATE_INVISIBLE);
  }
}

}  // namespace

namespace arc {

// This class keeps focus on a |ShellSurface| without interfering with default
// focus management in |ShellSurface|. For example, touch causes the
// |ShellSurface| to lose focus to its ancestor containing View.
class AXTreeSourceArc::FocusStealer : public views::View {
 public:
  explicit FocusStealer(int32_t id) : id_(id) { set_owned_by_client(); }

  void Steal() {
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    RequestFocus();
  }

  // views::View overrides.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->AddIntAttribute(ui::AX_ATTR_CHILD_TREE_ID, id_);
    node_data->role = ui::AX_ROLE_CLIENT;
  }

 private:
  const int32_t id_;
  DISALLOW_COPY_AND_ASSIGN(FocusStealer);
};

AXTreeSourceArc::AXTreeSourceArc(Delegate* delegate)
    : current_tree_serializer_(new AXTreeArcSerializer(this)),
      root_id_(-1),
      window_id_(-1),
      focused_node_id_(-1),
      is_notification_(false),
      delegate_(delegate),
      focus_stealer_(new FocusStealer(tree_id())) {}

AXTreeSourceArc::~AXTreeSourceArc() {
  Reset();
}

void AXTreeSourceArc::NotifyAccessibilityEvent(
    mojom::AccessibilityEventData* event_data) {
  tree_map_.clear();
  parent_map_.clear();
  root_id_ = -1;

  window_id_ = event_data->window_id;
  is_notification_ = event_data->notification_key.has_value();

  for (size_t i = 0; i < event_data->node_data.size(); ++i) {
    if (!event_data->node_data[i]->int_list_properties)
      continue;
    auto it = event_data->node_data[i]->int_list_properties->find(
        arc::mojom::AccessibilityIntListProperty::CHILD_NODE_IDS);
    if (it != event_data->node_data[i]->int_list_properties->end()) {
      for (size_t j = 0; j < it->second.size(); ++j)
        parent_map_[it->second[j]] = event_data->node_data[i]->id;
    }
  }

  for (size_t i = 0; i < event_data->node_data.size(); ++i) {
    int32_t id = event_data->node_data[i]->id;
    tree_map_[id] = event_data->node_data[i].get();
    if (parent_map_.find(id) == parent_map_.end()) {
      CHECK_EQ(-1, root_id_) << "Duplicated root";
      root_id_ = id;
    }

    if (GetBooleanProperty(event_data->node_data[i].get(),
                           arc::mojom::AccessibilityBooleanProperty::FOCUSED)) {
      focused_node_id_ = id;
    }
  }

  ExtensionMsg_AccessibilityEventParams params;
  params.event_type = ToAXEvent(event_data->event_type);

  params.tree_id = tree_id();
  params.id = event_data->source_id;

  ui::AXTreeUpdate update;

  if (event_data->event_type ==
      arc::mojom::AccessibilityEventType::WINDOW_CONTENT_CHANGED) {
    current_tree_serializer_->DeleteClientSubtree(
        GetFromId(event_data->source_id));
  }

  current_tree_serializer_->SerializeChanges(GetFromId(event_data->source_id),
                                             &params.update);

  extensions::AutomationEventRouter* router =
      extensions::AutomationEventRouter::GetInstance();
  router->DispatchAccessibilityEvent(params);
}

void AXTreeSourceArc::NotifyActionResult(const ui::AXActionData& data,
                                         bool result) {
  extensions::AutomationEventRouter::GetInstance()->DispatchActionResult(
      data, result);
}

void AXTreeSourceArc::Focus(aura::Window* window) {
  if (focus_stealer_->HasFocus())
    return;

  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (!widget || !widget->GetContentsView())
    return;

  views::View* view = widget->GetContentsView();
  if (!view->Contains(focus_stealer_.get()))
    view->AddChildView(focus_stealer_.get());
  focus_stealer_->Steal();
}

bool AXTreeSourceArc::GetTreeData(ui::AXTreeData* data) const {
  data->tree_id = tree_id();
  if (focused_node_id_ >= 0)
    data->focus_id = focused_node_id_;
  else if (root_id_ >= 0)
    data->focus_id = root_id_;
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
  if (!node || !node->int_list_properties)
    return;

  auto it = node->int_list_properties->find(
      arc::mojom::AccessibilityIntListProperty::CHILD_NODE_IDS);
  if (it == node->int_list_properties->end())
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

  int32_t id = node->id;
  out_data->id = id;
  if (id == root_id_)
    out_data->role = ui::AX_ROLE_ROOT_WEB_AREA;
  else
    PopulateAXRole(node, out_data);

  using AXIntListProperty = arc::mojom::AccessibilityIntListProperty;
  using AXIntProperty = arc::mojom::AccessibilityIntProperty;
  using AXStringListProperty = arc::mojom::AccessibilityStringListProperty;
  using AXStringProperty = arc::mojom::AccessibilityStringProperty;

  // String properties.
  std::string text;
  if (GetStringProperty(node, AXStringProperty::TEXT, &text))
    out_data->SetName(text);
  else if (GetStringProperty(node, AXStringProperty::CONTENT_DESCRIPTION,
                             &text))
    out_data->SetName(text);
  std::string role_description;
  if (GetStringProperty(node, AXStringProperty::ROLE_DESCRIPTION,
                        &role_description)) {
    out_data->AddStringAttribute(ui::AX_ATTR_ROLE_DESCRIPTION,
                                 role_description);
  }

  // Boolean properties.
  PopulateAXState(node, out_data);
  if (GetBooleanProperty(
          node, arc::mojom::AccessibilityBooleanProperty::SCROLLABLE)) {
    out_data->AddBoolAttribute(ui::AX_ATTR_SCROLLABLE, true);
  }
  if (GetBooleanProperty(node,
                         arc::mojom::AccessibilityBooleanProperty::CLICKABLE)) {
    out_data->AddBoolAttribute(ui::AX_ATTR_CLICKABLE, true);
  }

  exo::WMHelper* wm_helper =
      exo::WMHelper::HasInstance() ? exo::WMHelper::GetInstance() : nullptr;

  // To get bounds of a node which can be passed to AXNodeData.location,
  // - Root node must exist.
  // - Window where this tree is attached to need to be focused.
  if (root_id_ != -1 && wm_helper) {
    aura::Window* focused_window =
        is_notification_ ? nullptr : wm_helper->GetFocusedWindow();
    const gfx::Rect local_bounds = GetBounds(node, focused_window);
    out_data->location.SetRect(local_bounds.x(), local_bounds.y(),
                               local_bounds.width(), local_bounds.height());
  }

  if (out_data->role == ui::AX_ROLE_TEXT_FIELD && !text.empty())
    out_data->AddStringAttribute(ui::AX_ATTR_VALUE, text);

  // Integer properties.
  int32_t val;
  if (GetIntProperty(node, AXIntProperty::TEXT_SELECTION_START, &val) &&
      val >= 0)
    out_data->AddIntAttribute(ui::AX_ATTR_TEXT_SEL_START, val);

  if (GetIntProperty(node, AXIntProperty::TEXT_SELECTION_END, &val) && val >= 0)
    out_data->AddIntAttribute(ui::AX_ATTR_TEXT_SEL_END, val);

  // Custom actions.
  std::vector<int32_t> custom_action_ids;
  if (GetIntListProperty(node, AXIntListProperty::CUSTOM_ACTION_IDS,
                         &custom_action_ids)) {
    std::vector<std::string> custom_action_descriptions;

    CHECK(GetStringListProperty(
        node, AXStringListProperty::CUSTOM_ACTION_DESCRIPTIONS,
        &custom_action_descriptions));
    CHECK(!custom_action_ids.empty());
    CHECK_EQ(custom_action_ids.size(), custom_action_descriptions.size());

    out_data->AddAction(ui::AX_ACTION_CUSTOM_ACTION);
    out_data->AddIntListAttribute(ui::AX_ATTR_CUSTOM_ACTION_IDS,
                                  custom_action_ids);
    out_data->AddStringListAttribute(ui::AX_ATTR_CUSTOM_ACTION_DESCRIPTIONS,
                                     custom_action_descriptions);
  }
}

const gfx::Rect AXTreeSourceArc::GetBounds(
    mojom::AccessibilityNodeInfoData* node,
    aura::Window* focused_window) const {
  DCHECK_NE(root_id_, -1);

  gfx::Rect node_bounds = node->bounds_in_screen;

  if (focused_window && node->id == root_id_) {
    // Top level window returns its bounds in dip.
    aura::Window* toplevel_window = focused_window->GetToplevelWindow();
    float scale = toplevel_window->layer()->device_scale_factor();

    // Bounds of root node is relative to its container, i.e. focused window.
    node_bounds.Offset(
        static_cast<int>(-1.0f * scale *
                         static_cast<float>(toplevel_window->bounds().x())),
        static_cast<int>(-1.0f * scale *
                         static_cast<float>(toplevel_window->bounds().y())));

    return node_bounds;
  }

  // Bounds of non-root node is relative to its tree's root.
  gfx::Rect root_bounds = GetFromId(root_id_)->bounds_in_screen;
  node_bounds.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());
  return node_bounds;
}

void AXTreeSourceArc::PerformAction(const ui::AXActionData& data) {
  delegate_->OnAction(data);
}

void AXTreeSourceArc::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  current_tree_serializer_.reset(new AXTreeArcSerializer(this));
  root_id_ = -1;
  focused_node_id_ = -1;
  if (focus_stealer_->parent()) {
    views::View* parent = focus_stealer_->parent();
    parent->RemoveChildView(focus_stealer_.get());
    parent->NotifyAccessibilityEvent(ui::AX_EVENT_CHILDREN_CHANGED, false);
  }
  focus_stealer_.reset();
  extensions::AutomationEventRouter* router =
      extensions::AutomationEventRouter::GetInstance();
  if (!router)
    return;

  router->DispatchTreeDestroyedEvent(tree_id(), nullptr);
}

}  // namespace arc
