// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <stddef.h>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chromecast/common/extensions_api/cast_extension_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/accessibility/accessibility_alert_window.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using content::BrowserContext;
using extensions::cast::AutomationEventRouter;

namespace {

// Returns default browser context for sending events in case it was not
// provided.
BrowserContext* GetDefaultEventContext() {
  // TODO(rmrossi) Temporary stub
  return nullptr;
}

}  // namespace

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  return base::Singleton<AutomationManagerAura>::get();
}

void AutomationManagerAura::Enable(BrowserContext* context) {
  enabled_ = true;
  Reset(false);

  SendEvent(context, current_tree_->GetRoot(), ax::mojom::Event::kLoadComplete);
  views::AXAuraObjCache::GetInstance()->SetDelegate(this);

  aura::Window* active_window =
      chromecast::shell::CastBrowserProcess::GetInstance()
          ->accessibility_manager()
          ->window_tree_host()
          ->window();
  if (active_window) {
    views::AXAuraObjWrapper* focus =
        views::AXAuraObjCache::GetInstance()->GetOrCreate(active_window);
    SendEvent(context, focus, ax::mojom::Event::kChildrenChanged);
  }
}

void AutomationManagerAura::Disable() {
  enabled_ = false;
  Reset(true);
}

void AutomationManagerAura::HandleEvent(BrowserContext* context,
                                        views::View* view,
                                        ax::mojom::Event event_type) {
  if (!enabled_)
    return;

  views::AXAuraObjWrapper* aura_obj =
      view ? views::AXAuraObjCache::GetInstance()->GetOrCreate(view)
           : current_tree_->GetRoot();
  SendEvent(nullptr, aura_obj, event_type);
}

void AutomationManagerAura::HandleAlert(content::BrowserContext* context,
                                        const std::string& text) {
  if (alert_window_.get())
    alert_window_->HandleAlert(text);
}

void AutomationManagerAura::PerformAction(const ui::AXActionData& data) {
  CHECK(enabled_);

  current_tree_->HandleAccessibleAction(data);
}

void AutomationManagerAura::OnChildWindowRemoved(
    views::AXAuraObjWrapper* parent) {
  if (!enabled_)
    return;

  if (!parent)
    parent = current_tree_->GetRoot();

  SendEvent(nullptr, parent, ax::mojom::Event::kChildrenChanged);
}

void AutomationManagerAura::OnEvent(views::AXAuraObjWrapper* aura_obj,
                                    ax::mojom::Event event_type) {
  SendEvent(nullptr, aura_obj, event_type);
}

AutomationManagerAura::AutomationManagerAura()
    : enabled_(false), processing_events_(false) {}

AutomationManagerAura::~AutomationManagerAura() {}

void AutomationManagerAura::Reset(bool reset_serializer) {
  if (!current_tree_) {
    desktop_root_ = std::make_unique<AXRootObjWrapper>(this);
    current_tree_ =
        std::make_unique<AXTreeSourceAura>(desktop_root_.get(), ax_tree_id());
  }
  if (reset_serializer) {
    current_tree_serializer_.reset();
    alert_window_.reset();
  } else {
    current_tree_serializer_ =
        std::make_unique<AuraAXTreeSerializer>(current_tree_.get());
#if defined(OS_CHROMEOS)
    ash::Shell* shell = ash::Shell::Get();
    // Windows within the overlay container get moved to the new monitor when
    // the primary display gets swapped.
    alert_window_ = std::make_unique<views::AccessibilityAlertWindow>(
        shell->GetContainer(shell->GetPrimaryRootWindow(),
                            ash::kShellWindowId_OverlayContainer),
        views::AXAuraObjCache::GetInstance());
#endif  // defined(OS_CHROMEOS)
  }
}

void AutomationManagerAura::SendEvent(BrowserContext* context,
                                      views::AXAuraObjWrapper* aura_obj,
                                      ax::mojom::Event event_type) {
  if (!current_tree_serializer_)
    return;

  if (!context)
    context = GetDefaultEventContext();

  if (processing_events_) {
    pending_events_.push_back(std::make_pair(aura_obj, event_type));
    return;
  }
  processing_events_ = true;

  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = ax_tree_id();
  event_bundle.mouse_location = aura::Env::GetInstance()->last_mouse_location();

  ui::AXTreeUpdate update;
  if (!current_tree_serializer_->SerializeChanges(aura_obj, &update)) {
    LOG(ERROR) << "Unable to serialize one accessibility event.";
    return;
  }
  event_bundle.updates.push_back(update);

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus =
      views::AXAuraObjCache::GetInstance()->GetFocus();
  if (focus) {
    ui::AXTreeUpdate focused_node_update;
    current_tree_serializer_->SerializeChanges(focus, &focused_node_update);
    event_bundle.updates.push_back(focused_node_update);
  }

  ui::AXEvent event;
  event.id = aura_obj->GetUniqueId();
  event.event_type = event_type;
  event_bundle.events.push_back(event);

  AutomationEventRouter* router = AutomationEventRouter::GetInstance();
  router->DispatchAccessibilityEvents(event_bundle);

  processing_events_ = false;
  auto pending_events_copy = pending_events_;
  pending_events_.clear();
  for (size_t i = 0; i < pending_events_copy.size(); ++i) {
    SendEvent(context, pending_events_copy[i].first,
              pending_events_copy[i].second);
  }
}
