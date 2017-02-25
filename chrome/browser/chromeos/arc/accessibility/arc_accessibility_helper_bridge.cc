// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// This class keeps focus on a |ShellSurface| without interfering with default
// focus management in |ShellSurface|. For example, touch causes the
// |ShellSurface| to lose focus to its ancestor containing View.
class FocusStealer : public views::View {
 public:
  explicit FocusStealer(int32_t id) : id_(id) {
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    set_owned_by_client();
  }

  // views::View overrides.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->AddIntAttribute(ui::AX_ATTR_CHILD_TREE_ID, id_);
    node_data->role = ui::AX_ROLE_CLIENT;
  }

 private:
  int32_t id_;
  DISALLOW_COPY_AND_ASSIGN(FocusStealer);
};

exo::Surface* GetArcSurface(const aura::Window* window) {
  if (!window)
    return nullptr;

  exo::Surface* arc_surface = exo::Surface::AsSurface(window);
  if (!arc_surface)
    arc_surface = exo::ShellSurface::GetMainSurface(window);
  return arc_surface;
}

void DispatchFocusChange(arc::mojom::AccessibilityNodeInfoData* node_data) {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager)
    return;

  exo::WMHelper* wmHelper = exo::WMHelper::GetInstance();
  aura::Window* focused_window = wmHelper->GetFocusedWindow();
  if (!focused_window)
    return;

  aura::Window* toplevel_window = focused_window->GetToplevelWindow();

  gfx::Rect bounds_in_screen = gfx::ScaleToEnclosingRect(
      node_data->boundsInScreen,
      1.0f / toplevel_window->layer()->device_scale_factor());

  accessibility_manager->OnViewFocusedInArc(bounds_in_screen);
}

}  // namespace

namespace arc {

ArcAccessibilityHelperBridge::ArcAccessibilityHelperBridge(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->accessibility_helper()->AddObserver(this);
}

ArcAccessibilityHelperBridge::~ArcAccessibilityHelperBridge() {
  arc_bridge_service()->accessibility_helper()->RemoveObserver(this);
}

void ArcAccessibilityHelperBridge::OnInstanceReady() {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->accessibility_helper(), Init);
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableChromeVoxArcSupport)) {
    instance->SetFilter(arc::mojom::AccessibilityFilterType::ALL);
    if (!tree_source_) {
      tree_source_.reset(new AXTreeSourceArc(tree_id()));
      focus_stealer_.reset(new FocusStealer(tree_source_->tree_id()));
      exo::WMHelper::GetInstance()->AddActivationObserver(this);
    }
  } else if (accessibility_manager &&
             accessibility_manager->IsFocusHighlightEnabled()) {
    instance->SetFilter(arc::mojom::AccessibilityFilterType::FOCUS);
  }
}

void ArcAccessibilityHelperBridge::OnAccessibilityEventDeprecated(
    mojom::AccessibilityEventType event_type,
    mojom::AccessibilityNodeInfoDataPtr event_source) {
  if (event_type == arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    DispatchFocusChange(event_source.get());
}

void ArcAccessibilityHelperBridge::OnAccessibilityEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  if (tree_source_) {
    tree_source_->NotifyAccessibilityEvent(event_data.get());
    return;
  }

  if (event_data->eventType != arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    return;

  CHECK_EQ(1U, event_data.get()->nodeData.size());
  DispatchFocusChange(event_data.get()->nodeData[0].get());
}

void ArcAccessibilityHelperBridge::OnWindowActivated(
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active == lost_active || !tree_source_)
    return;

  exo::Surface* active_surface = GetArcSurface(gained_active);
  exo::Surface* inactive_surface = GetArcSurface(lost_active);

  // Detach the accessibility tree from an inactive ShellSurface so that any
  // client walking the desktop tree gets non-duplicated linearization.
  if (inactive_surface) {
    views::Widget* widget = views::Widget::GetWidgetForNativeView(lost_active);
    if (widget && widget->GetContentsView()) {
      views::View* view = widget->GetContentsView();
      view->RemoveChildView(focus_stealer_.get());
      view->NotifyAccessibilityEvent(ui::AX_EVENT_CHILDREN_CHANGED, false);
    }
  }

  if (!active_surface)
    return;

  views::Widget* widget = views::Widget::GetWidgetForNativeView(gained_active);
  if (widget && widget->GetContentsView()) {
    views::View* view = widget->GetContentsView();
    if (!view->Contains(focus_stealer_.get()))
      view->AddChildView(focus_stealer_.get());
    focus_stealer_->RequestFocus();
    view->NotifyAccessibilityEvent(ui::AX_EVENT_CHILDREN_CHANGED, false);
  }
}

void ArcAccessibilityHelperBridge::PerformAction(const ui::AXActionData& data) {
  arc::mojom::AccessibilityActionType mojo_action;
  switch (data.action) {
    case ui::AX_ACTION_DO_DEFAULT:
      mojo_action = arc::mojom::AccessibilityActionType::CLICK;
      break;
    default:
      return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->accessibility_helper(), PerformAction);
  instance->PerformAction(data.target_node_id, mojo_action);
}

}  // namespace arc
