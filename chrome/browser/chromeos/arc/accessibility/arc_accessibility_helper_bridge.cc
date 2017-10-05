// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <utility>

#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "ui/arc/notification/arc_notification_surface.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/native/native_view_host.h"

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
  const std::string* arc_app_id = exo::ShellSurface::GetApplicationId(window);
  if (!arc_app_id)
    return kNoTaskId;

  int32_t task_id = kNoTaskId;
  if (sscanf(arc_app_id->c_str(), "org.chromium.arc.%d", &task_id) != 1)
    return kNoTaskId;

  return task_id;
}

void DispatchFocusChange(arc::mojom::AccessibilityNodeInfoData* node_data,
                         Profile* profile) {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager || accessibility_manager->profile() != profile)
    return;

  exo::WMHelper* wm_helper = exo::WMHelper::GetInstance();
  if (!wm_helper)
    return;

  aura::Window* focused_window = wm_helper->GetFocusedWindow();
  if (!focused_window)
    return;

  aura::Window* toplevel_window = focused_window->GetToplevelWindow();

  gfx::Rect bounds_in_screen = gfx::ScaleToEnclosingRect(
      node_data->bounds_in_screen,
      1.0f / toplevel_window->layer()->device_scale_factor());

  accessibility_manager->OnViewFocusedInArc(bounds_in_screen);
}

arc::mojom::AccessibilityFilterType GetFilterTypeForProfile(Profile* profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableChromeVoxArcSupport)) {
    return arc::mojom::AccessibilityFilterType::ALL;
  }

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager)
    return arc::mojom::AccessibilityFilterType::OFF;

  // TODO(yawano): Support the case where primary user is in background.
  if (accessibility_manager->profile() != profile)
    return arc::mojom::AccessibilityFilterType::OFF;

  if (accessibility_manager->IsSelectToSpeakEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled()) {
    return arc::mojom::AccessibilityFilterType::ALL;
  }

  if (accessibility_manager->IsSpokenFeedbackEnabled())
    return arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME;

  if (accessibility_manager->IsFocusHighlightEnabled())
    return arc::mojom::AccessibilityFilterType::FOCUS;

  return arc::mojom::AccessibilityFilterType::OFF;
}

}  // namespace

namespace arc {

namespace {

// Singleton factory for ArcAccessibilityHelperBridge.
class ArcAccessibilityHelperBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAccessibilityHelperBridge,
          ArcAccessibilityHelperBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAccessibilityHelperBridgeFactory";

  static ArcAccessibilityHelperBridgeFactory* GetInstance() {
    return base::Singleton<ArcAccessibilityHelperBridgeFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      ArcAccessibilityHelperBridgeFactory>;

  ArcAccessibilityHelperBridgeFactory() {
    // ArcAccessibilityHelperBridge needs to track task creation and
    // destruction in the container, which are notified to ArcAppListPrefs
    // via Mojo.
    DependsOn(ArcAppListPrefsFactory::GetInstance());
  }
  ~ArcAccessibilityHelperBridgeFactory() override = default;
};

}  // namespace

// static
ArcAccessibilityHelperBridge*
ArcAccessibilityHelperBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAccessibilityHelperBridgeFactory::GetForBrowserContext(context);
}

ArcAccessibilityHelperBridge::ArcAccessibilityHelperBridge(
    content::BrowserContext* browser_context,
    ArcBridgeService* arc_bridge_service)
    : profile_(Profile::FromBrowserContext(browser_context)),
      arc_bridge_service_(arc_bridge_service),
      binding_(this),
      current_task_id_(kNoTaskId),
      fallback_tree_(new AXTreeSourceArc(this)) {
  arc_bridge_service_->accessibility_helper()->AddObserver(this);

  // Null on testing.
  auto* app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (app_list_prefs)
    app_list_prefs->AddObserver(this);
}

ArcAccessibilityHelperBridge::~ArcAccessibilityHelperBridge() = default;

void ArcAccessibilityHelperBridge::SetNativeChromeVoxArcSupport(bool enabled) {
  if (current_task_id_ == kNoTaskId)
    return;

  for (auto entry : package_name_to_task_ids_) {
    if (entry.second.count(current_task_id_) > 0) {
      auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service_->accessibility_helper(),
          SetNativeChromeVoxArcSupport);
      instance->SetNativeChromeVoxArcSupport(
          entry.first, enabled,
          base::Bind(&ArcAccessibilityHelperBridge::
                         OnSetNativeChromeVoxArcSupportProcessed,
                     base::Unretained(this), entry.first, enabled));
      break;
    }
  }
}

void ArcAccessibilityHelperBridge::OnSetNativeChromeVoxArcSupportProcessed(
    const std::string& package_name,
    bool enabled,
    bool processed) {
  if (!processed)
    return;

  auto it = package_name_to_tree_.find(package_name);
  if (enabled) {
    package_name_to_tree_[package_name].reset(new AXTreeSourceArc(this));
    package_name_to_tree_[package_name]->Focus(ash::wm::GetActiveWindow());
  } else if (it != package_name_to_tree_.end()) {
    package_name_to_tree_.erase(it);
  }
}

void ArcAccessibilityHelperBridge::Shutdown() {
  // We do not unregister ourselves from WMHelper as an ActivationObserver
  // because it is always null at this point during teardown.

  // Null on testing.
  auto* app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (app_list_prefs)
    app_list_prefs->RemoveObserver(this);

  arc_bridge_service_->accessibility_helper()->RemoveObserver(this);

  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (surface_manager)
    surface_manager->RemoveObserver(this);
}

void ArcAccessibilityHelperBridge::OnInstanceReady() {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), Init);
  DCHECK(instance);

  mojom::AccessibilityHelperHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  instance->Init(std::move(host_proxy));

  arc::mojom::AccessibilityFilterType filter_type =
      GetFilterTypeForProfile(profile_);
  instance->SetFilter(filter_type);

  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (surface_manager)
    surface_manager->AddObserver(this);

  if (filter_type == arc::mojom::AccessibilityFilterType::ALL ||
      filter_type ==
          arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME) {
    // TODO(yawano): Handle the case where filter_type has changed between
    // OFF/FOCUS and ALL/WHITELISTED_PACKAGE_NAME after this initialization.
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
  }
}

void ArcAccessibilityHelperBridge::OnAccessibilityEventDeprecated(
    mojom::AccessibilityEventType event_type,
    mojom::AccessibilityNodeInfoDataPtr event_source) {
  if (event_type == arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    DispatchFocusChange(event_source.get(), profile_);
}

void ArcAccessibilityHelperBridge::OnAccessibilityEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  // TODO(yawano): Handle AccessibilityFilterType::OFF.
  arc::mojom::AccessibilityFilterType filter_type =
      GetFilterTypeForProfile(profile_);

  if (filter_type == arc::mojom::AccessibilityFilterType::ALL ||
      filter_type ==
          arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME) {
    // Get the task id for this package which requires inspecting node data.
    if (event_data->node_data.empty())
      return;

    arc::mojom::AccessibilityNodeInfoData* node =
        event_data->node_data[0].get();
    if (!node->string_properties)
      return;

    AXTreeSourceArc* tree_source = nullptr;
    bool is_notification_event = event_data->notification_key.has_value();

    if (is_notification_event) {
      std::string notification_key = event_data->notification_key.value();
      if (event_data->event_type ==
          arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED) {
        tree_source = CreateFromNotificationKey(notification_key);
      } else {
        tree_source = GetFromNotificationKey(notification_key);
      }
    } else {
      auto package_entry = node->string_properties->find(
          arc::mojom::AccessibilityStringProperty::PACKAGE_NAME);
      if (package_entry == node->string_properties->end())
        return;

      tree_source = GetOrCreateFromPackageName(package_entry->second);
    }

    if (!tree_source)
      return;

    tree_source->NotifyAccessibilityEvent(event_data.get());

    auto* surface_manager = ArcNotificationSurfaceManager::Get();
    if (surface_manager && is_notification_event &&
        event_data->event_type ==
            arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED) {
      std::string notification_key = event_data->notification_key.value();
      ArcNotificationSurface* surface =
          surface_manager->GetArcSurface(notification_key);

      ui::AXTreeData tree_data;
      if (surface && tree_source->GetTreeData(&tree_data)) {
        surface->SetAXTreeId(tree_data.tree_id);

        // Dispatch AX_EVENT_CHILDREN_CHANGED to force AXNodeData of the
        // notification updated. Before AXTreeId is set, its AXNodeData is
        // populated as a button.
        if (surface->IsAttached()) {
          surface->GetAttachedHost()->NotifyAccessibilityEvent(
              ui::AX_EVENT_CHILDREN_CHANGED, false);
        }
      }
    }

    return;
  }

  if (event_data->event_type !=
      arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    return;

  CHECK_EQ(1U, event_data.get()->node_data.size());
  DispatchFocusChange(event_data.get()->node_data[0].get(), profile_);
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::GetOrCreateFromPackageName(
    const std::string& package_name) {
  auto task_ids_it = package_name_to_task_ids_.find(package_name);

  AXTreeSourceArc* tree_source;
  if (task_ids_it == package_name_to_task_ids_.end()) {
    // It's possible for there to have never been a package mapping for this
    // task id due to underlying bugs. See crbug.com/745978.
    for (auto entry : package_name_to_task_ids_) {
      if (entry.second.count(current_task_id_) > 0)
        return nullptr;
    }
    DLOG(ERROR) << "Package did not trigger OnTaskCreated " << package_name;
    tree_source = fallback_tree_.get();
  } else {
    const auto& task_ids = task_ids_it->second;

    // Reject updates to non-current task ids. We can do this currently
    // because all events include the entire tree.
    if (task_ids.count(current_task_id_) == 0)
      return nullptr;

    auto tree_it = package_name_to_tree_.find(package_name);
    if (tree_it == package_name_to_tree_.end()) {
      package_name_to_tree_[package_name].reset(new AXTreeSourceArc(this));
      tree_source = package_name_to_tree_[package_name].get();
    } else {
      tree_source = tree_it->second.get();
    }
  }

  return tree_source;
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::CreateFromNotificationKey(
    const std::string& notification_key) {
  CHECK_EQ(0U, notification_key_to_tree_.count(notification_key));

  notification_key_to_tree_[notification_key].reset(new AXTreeSourceArc(this));
  return notification_key_to_tree_[notification_key].get();
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::GetFromNotificationKey(
    const std::string& notification_key) const {
  auto tree_it = notification_key_to_tree_.find(notification_key);
  if (tree_it == notification_key_to_tree_.end())
    return nullptr;

  return tree_it->second.get();
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::GetFromTreeId(
    int32_t tree_id) const {
  for (auto package_name_it = package_name_to_tree_.begin();
       package_name_it != package_name_to_tree_.end(); ++package_name_it) {
    ui::AXTreeData tree_data;
    package_name_it->second->GetTreeData(&tree_data);
    if (tree_data.tree_id == tree_id)
      return package_name_it->second.get();
  }

  for (auto notification_it = notification_key_to_tree_.begin();
       notification_it != notification_key_to_tree_.end(); ++notification_it) {
    ui::AXTreeData tree_data;
    notification_it->second->GetTreeData(&tree_data);
    if (tree_data.tree_id == tree_id)
      return notification_it->second.get();
  }

  ui::AXTreeData tree_data;
  fallback_tree_->GetTreeData(&tree_data);
  if (tree_data.tree_id == tree_id)
    return fallback_tree_.get();

  return nullptr;
}

void ArcAccessibilityHelperBridge::OnAction(
    const ui::AXActionData& data) const {
  arc::mojom::AccessibilityActionDataPtr action_data =
      arc::mojom::AccessibilityActionData::New();

  action_data->node_id = data.target_node_id;

  AXTreeSourceArc* tree_source = GetFromTreeId(data.target_tree_id);
  if (!tree_source)
    return;
  action_data->window_id = tree_source->window_id();

  switch (data.action) {
    case ui::AX_ACTION_DO_DEFAULT:
      action_data->action_type = arc::mojom::AccessibilityActionType::CLICK;
      break;
    case ui::AX_ACTION_SCROLL_BACKWARD:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_BACKWARD;
      break;
    case ui::AX_ACTION_SCROLL_FORWARD:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_FORWARD;
      break;
    case ui::AX_ACTION_SCROLL_UP:
      action_data->action_type = arc::mojom::AccessibilityActionType::SCROLL_UP;
      break;
    case ui::AX_ACTION_SCROLL_DOWN:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_DOWN;
      break;
    case ui::AX_ACTION_SCROLL_LEFT:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_LEFT;
      break;
    case ui::AX_ACTION_SCROLL_RIGHT:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_RIGHT;
      break;
    case ui::AX_ACTION_CUSTOM_ACTION:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::CUSTOM_ACTION;
      action_data->custom_action_id = data.custom_action_id;
      break;
    default:
      return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), PerformAction);
  instance->PerformAction(
      std::move(action_data),
      base::Bind(&ArcAccessibilityHelperBridge::OnActionResult,
                 base::Unretained(this), data));
}

void ArcAccessibilityHelperBridge::OnActionResult(const ui::AXActionData& data,
                                                  bool result) const {
  AXTreeSourceArc* tree_source = GetFromTreeId(data.target_tree_id);

  if (!tree_source)
    return;

  tree_source->NotifyActionResult(data, result);
}

void ArcAccessibilityHelperBridge::OnWindowActivated(
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active == lost_active)
    return;

  if (!GetArcSurface(gained_active))
    return;

  // First, do a lookup for the task id associated with this app. There should
  // always be a valid entry.
  int32_t task_id = GetTaskId(gained_active);
  const std::pair<const std::string, std::set<int32_t>>* found_entry = nullptr;
  for (const auto& task_ids_entry : package_name_to_task_ids_) {
    if (task_ids_entry.second.count(task_id) != 0) {
      found_entry = &task_ids_entry;
      break;
    }
  }

  // No valid entry found; this can happen if |OnTaskCreated| never got called
  // for a task. Use the fallback tree if native ARC++ access is on for all
  // apps.
  if (!found_entry) {
    bool should_pass_through = GetFilterTypeForProfile(profile_) !=
                               arc::mojom::AccessibilityFilterType::ALL;
    gained_active->SetProperty(
        aura::client::kAccessibilityTouchExplorationPassThrough,
        should_pass_through);
    if (!should_pass_through)
      fallback_tree_->Focus(gained_active);
    return;
  }

  // Finally, do a lookup for the tree source. A tree source may not exist
  // because the app isn't whitelisted Android side or no data has been received
  // for the app.
  auto it = package_name_to_tree_.find(found_entry->first);
  if (it != package_name_to_tree_.end()) {
    gained_active->SetProperty(
        aura::client::kAccessibilityTouchExplorationPassThrough, false);
    it->second->Focus(gained_active);
  } else {
    gained_active->SetProperty(
        aura::client::kAccessibilityTouchExplorationPassThrough, true);
  }
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

void ArcAccessibilityHelperBridge::OnNotificationSurfaceAdded(
    ArcNotificationSurface* surface) {
  const std::string& notification_key = surface->GetNotificationKey();
  auto tree_it = notification_key_to_tree_.find(notification_key);
  if (tree_it == notification_key_to_tree_.end())
    return;

  ui::AXTreeData tree_data;
  if (!tree_it->second->GetTreeData(&tree_data))
    return;

  surface->SetAXTreeId(tree_data.tree_id);

  // Dispatch AX_EVENT_CHILDREN_CHANGED to force AXNodeData of the notification
  // updated. As order of OnNotificationSurfaceAdded call is not guaranteed, we
  // are dispatching the event in both ArcAccessibilityHelperBridge and
  // ArcNotificationContentView. The event needs to be dispatched after 1. ax
  // tree id is set to the surface, 2 the surface is attached to the content
  // view.
  if (surface->IsAttached()) {
    surface->GetAttachedHost()->NotifyAccessibilityEvent(
        ui::AX_EVENT_CHILDREN_CHANGED, false);
  }
}

void ArcAccessibilityHelperBridge::OnNotificationSurfaceRemoved(
    ArcNotificationSurface* surface) {
  const std::string& notification_key = surface->GetNotificationKey();
  notification_key_to_tree_.erase(notification_key);
}

}  // namespace arc
