// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <utility>

#include "base/command_line.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace {

constexpr int32_t kNoTaskId = -1;

exo::Surface* GetArcSurface(const aura::Window* window) {
  if (!window)
    return nullptr;

  exo::Surface* arc_surface = exo::Surface::AsSurface(window);
  if (!arc_surface)
    arc_surface = exo::ShellSurface::GetMainSurface(window);
  return arc_surface;
}

int32_t GetTaskId(aura::Window* window) {
  const std::string arc_app_id = exo::ShellSurface::GetApplicationId(window);
  if (arc_app_id.empty())
    return kNoTaskId;

  int32_t task_id = kNoTaskId;
  if (sscanf(arc_app_id.c_str(), "org.chromium.arc.%d", &task_id) != 1)
    return kNoTaskId;

  return task_id;
}

void DispatchFocusChange(arc::mojom::AccessibilityNodeInfoData* node_data) {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager)
    return;

  exo::WMHelper* wm_helper = exo::WMHelper::GetInstance();
  if (!wm_helper)
    return;

  aura::Window* focused_window = wm_helper->GetFocusedWindow();
  if (!focused_window)
    return;

  aura::Window* toplevel_window = focused_window->GetToplevelWindow();

  gfx::Rect bounds_in_screen = gfx::ScaleToEnclosingRect(
      node_data->boundsInScreen,
      1.0f / toplevel_window->layer()->device_scale_factor());

  accessibility_manager->OnViewFocusedInArc(bounds_in_screen);
}

arc::mojom::AccessibilityFilterType GetFilterType() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableChromeVoxArcSupport)) {
    return arc::mojom::AccessibilityFilterType::ALL;
  }

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager)
    return arc::mojom::AccessibilityFilterType::OFF;

  if (accessibility_manager->IsSpokenFeedbackEnabled())
    return arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME;

  if (accessibility_manager->IsFocusHighlightEnabled())
    return arc::mojom::AccessibilityFilterType::FOCUS;

  return arc::mojom::AccessibilityFilterType::OFF;
}

}  // namespace

namespace arc {

ArcAccessibilityHelperBridge::ArcAccessibilityHelperBridge(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), current_task_id_(kNoTaskId) {
  arc_bridge_service()->accessibility_helper()->AddObserver(this);
}

ArcAccessibilityHelperBridge::~ArcAccessibilityHelperBridge() {
  // We do not unregister ourselves from WMHelper as an ActivationObserver
  // because it is always null at this point during teardown.

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (accessibility_manager) {
    Profile* profile = accessibility_manager->profile();
    if (profile && ArcAppListPrefs::Get(profile))
      ArcAppListPrefs::Get(profile)->RemoveObserver(this);
  }

  ArcBridgeService* arc_bridge_service_ptr = arc_bridge_service();
  if (arc_bridge_service_ptr)
    arc_bridge_service_ptr->accessibility_helper()->RemoveObserver(this);
}

void ArcAccessibilityHelperBridge::OnInstanceReady() {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager)
    return;

  Profile* profile = accessibility_manager->profile();
  if (!profile)
    return;

  ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(profile);
  if (!arc_app_list_prefs)
    return;

  arc_app_list_prefs->AddObserver(this);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->accessibility_helper(), Init);
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());

  arc::mojom::AccessibilityFilterType filter_type = GetFilterType();
  instance->SetFilter(filter_type);

  if (filter_type == arc::mojom::AccessibilityFilterType::ALL ||
      filter_type ==
          arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME) {
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
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
  arc::mojom::AccessibilityFilterType filter_type = GetFilterType();

  if (filter_type == arc::mojom::AccessibilityFilterType::ALL ||
      filter_type ==
          arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME) {
    // Get the task id for this package which requires inspecting node data.
    if (event_data->nodeData.empty())
      return;

    arc::mojom::AccessibilityNodeInfoData* node = event_data->nodeData[0].get();
    if (!node->stringProperties)
      return;

    auto package_it = node->stringProperties->find(
        arc::mojom::AccessibilityStringProperty::PACKAGE_NAME);
    if (package_it == node->stringProperties->end())
      return;

    auto task_ids_it = package_name_to_task_ids_.find(package_it->second);
    if (task_ids_it == package_name_to_task_ids_.end())
      return;

    const auto& task_ids = task_ids_it->second;

    // Reject updates to non-current task ids. We can do this currently
    // because all events include the entire tree.
    if (task_ids.count(current_task_id_) == 0)
      return;

    auto tree_it = package_name_to_tree_.find(package_it->second);
    AXTreeSourceArc* tree_source;
    if (tree_it == package_name_to_tree_.end()) {
      package_name_to_tree_[package_it->second].reset(
          new AXTreeSourceArc(this));
      tree_source = package_name_to_tree_[package_it->second].get();
    } else {
      tree_source = tree_it->second.get();
    }
    tree_source->NotifyAccessibilityEvent(event_data.get());
    return;
  }

  if (event_data->eventType != arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    return;

  CHECK_EQ(1U, event_data.get()->nodeData.size());
  DispatchFocusChange(event_data.get()->nodeData[0].get());
}

void ArcAccessibilityHelperBridge::OnAction(
    const ui::AXActionData& data) const {
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

void ArcAccessibilityHelperBridge::OnWindowActivated(
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active == lost_active)
    return;

  if (!GetArcSurface(gained_active))
    return;

  // Grab the tree source associated with this app.
  int32_t task_id = GetTaskId(gained_active);
  const std::pair<const std::string, std::set<int32_t>>* found_entry = nullptr;
  for (const auto& task_ids_entry : package_name_to_task_ids_) {
    if (task_ids_entry.second.count(task_id) != 0) {
      found_entry = &task_ids_entry;
      break;
    }
  }

  if (!found_entry)
    return;

  auto it = package_name_to_tree_.find(found_entry->first);
  if (it != package_name_to_tree_.end())
    it->second->Focus(gained_active);
}

void ArcAccessibilityHelperBridge::OnTaskCreated(
    int32_t task_id,
    const std::string& package_name,
    const std::string& activity,
    const std::string& intent) {
  package_name_to_task_ids_[package_name].insert(task_id);
}

void ArcAccessibilityHelperBridge::OnTaskDestroyed(int32_t task_id) {
  for (auto& task_ids_entry : package_name_to_task_ids_) {
    if (task_ids_entry.second.erase(task_id) > 0) {
      if (task_ids_entry.second.empty()) {
        package_name_to_tree_.erase(task_ids_entry.first);
        package_name_to_task_ids_.erase(task_ids_entry.first);
      }
      break;
    }
  }
}

void ArcAccessibilityHelperBridge::OnTaskSetActive(int32_t task_id) {
  current_task_id_ = task_id;
}

}  // namespace arc
