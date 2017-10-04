// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/exo/wm_helper.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/accessibility/ax_host_delegate.h"
#include "ui/arc/notification/arc_notification_surface_manager.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class AXTreeSourceArc;
class ArcBridgeService;

// ArcAccessibilityHelperBridge is an instance to receive converted Android
// accessibility events and info via mojo interface and dispatch them to chrome
// os components.
class ArcAccessibilityHelperBridge
    : public KeyedService,
      public mojom::AccessibilityHelperHost,
      public InstanceHolder<mojom::AccessibilityHelperInstance>::Observer,
      public exo::WMHelper::ActivationObserver,
      public AXTreeSourceArc::Delegate,
      public ArcAppListPrefs::Observer,
      public ArcNotificationSurfaceManager::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAccessibilityHelperBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcAccessibilityHelperBridge(content::BrowserContext* browser_context,
                               ArcBridgeService* arc_bridge_service);
  ~ArcAccessibilityHelperBridge() override;

  // Sets ChromeVox or TalkBack active for the current task.
  void SetNativeChromeVoxArcSupport(bool enabled);

  // Receives the result of setting native ChromeVox Arc support.
  void OnSetNativeChromeVoxArcSupportProcessed(const std::string& package_name,
                                               bool enabled,
                                               bool processed);

  // KeyedService overrides.
  void Shutdown() override;

  // InstanceHolder<mojom::AccessibilityHelperInstance>::Observer overrides.
  void OnInstanceReady() override;

  // mojom::AccessibilityHelperHost overrides.
  void OnAccessibilityEventDeprecated(
      mojom::AccessibilityEventType event_type,
      mojom::AccessibilityNodeInfoDataPtr event_source) override;
  void OnAccessibilityEvent(
      mojom::AccessibilityEventDataPtr event_data) override;

  // AXTreeSourceArc::Delegate overrides.
  void OnAction(const ui::AXActionData& data) const override;

  // ArcAppListPrefs::Observer overrides.
  void OnTaskCreated(int task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnTaskSetActive(int32_t task_id) override;

  // ArcNotificationSurfaceManager::Observer overrides.
  void OnNotificationSurfaceAdded(ArcNotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(ArcNotificationSurface* surface) override;

  const std::map<std::string, std::unique_ptr<AXTreeSourceArc>>&
  package_name_to_tree_for_test() {
    return package_name_to_tree_;
  }
  const std::map<std::string, std::set<int32_t>>&
  package_name_to_task_ids_for_test() {
    return package_name_to_task_ids_;
  }

 private:
  // exo::WMHelper::ActivationObserver overrides.
  void OnWindowActivated(aura::Window* gained_active,
                         aura::Window* lost_active) override;

  void OnActionResult(const ui::AXActionData& data, bool result) const;

  AXTreeSourceArc* GetOrCreateFromPackageName(const std::string& package_name);
  AXTreeSourceArc* CreateFromNotificationKey(
      const std::string& notification_key);
  AXTreeSourceArc* GetFromNotificationKey(
      const std::string& notification_key) const;
  AXTreeSourceArc* GetFromTreeId(int32_t tree_id) const;

  Profile* const profile_;
  ArcBridgeService* const arc_bridge_service_;
  mojo::Binding<mojom::AccessibilityHelperHost> binding_;
  std::map<std::string, std::unique_ptr<AXTreeSourceArc>> package_name_to_tree_;
  std::map<std::string, std::unique_ptr<AXTreeSourceArc>>
      notification_key_to_tree_;
  std::map<std::string, std::set<int32_t>> package_name_to_task_ids_;
  int32_t current_task_id_;
  std::unique_ptr<AXTreeSourceArc> fallback_tree_;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
