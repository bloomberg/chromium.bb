// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <stddef.h>

#include <vector>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_context.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/wm/window_util.h"  // nogncheck
#endif

using content::BrowserContext;
using extensions::AutomationEventRouter;

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  return base::Singleton<AutomationManagerAura>::get();
}

void AutomationManagerAura::Enable(BrowserContext* context) {
  enabled_ = true;
  if (!current_tree_.get())
    current_tree_.reset(new AXTreeSourceAura());
  ResetSerializer();

  SendEvent(context, current_tree_->GetRoot(), ui::AX_EVENT_LOAD_COMPLETE);
  views::AXAuraObjCache::GetInstance()->SetDelegate(this);

#if defined(OS_CHROMEOS)
  aura::Window* active_window = ash::wm::GetActiveWindow();
  if (active_window) {
    views::AXAuraObjWrapper* focus =
        views::AXAuraObjCache::GetInstance()->GetOrCreate(active_window);
    SendEvent(context, focus, ui::AX_EVENT_CHILDREN_CHANGED);
  }
#endif
}

void AutomationManagerAura::Disable() {
  enabled_ = false;

  // Reset the serializer to save memory.
  current_tree_serializer_->Reset();
}

void AutomationManagerAura::HandleEvent(BrowserContext* context,
                                        views::View* view,
                                        ui::AXEvent event_type) {
  if (!enabled_)
    return;

  views::AXAuraObjWrapper* aura_obj = view ?
      views::AXAuraObjCache::GetInstance()->GetOrCreate(view) :
      current_tree_->GetRoot();
  SendEvent(nullptr, aura_obj, event_type);
}

void AutomationManagerAura::HandleAlert(content::BrowserContext* context,
                                        const std::string& text) {
  if (!enabled_)
    return;

  views::AXAuraObjWrapper* obj =
      static_cast<AXRootObjWrapper*>(current_tree_->GetRoot())
          ->GetAlertForText(text);
  SendEvent(context, obj, ui::AX_EVENT_ALERT);
}

void AutomationManagerAura::PerformAction(
    const ui::AXActionData& data) {
  CHECK(enabled_);

  switch (data.action) {
    case ui::AX_ACTION_DO_DEFAULT:
      current_tree_->DoDefault(data.target_node_id);
      break;
    case ui::AX_ACTION_FOCUS:
      current_tree_->Focus(data.target_node_id);
      break;
    case ui::AX_ACTION_SCROLL_TO_MAKE_VISIBLE:
      current_tree_->MakeVisible(data.target_node_id);
      break;
    case ui::AX_ACTION_SET_SELECTION:
      if (data.anchor_node_id != data.focus_node_id) {
        NOTREACHED();
        return;
      }
      current_tree_->SetSelection(
          data.anchor_node_id, data.anchor_offset, data.focus_offset);
      break;
    case ui::AX_ACTION_SHOW_CONTEXT_MENU:
      current_tree_->ShowContextMenu(data.target_node_id);
      break;
    case ui::AX_ACTION_SET_ACCESSIBILITY_FOCUS:
      // Sent by ChromeVox but doesn't need to be handled by aura.
      break;
    case ui::AX_ACTION_SET_SEQUENTIAL_FOCUS_NAVIGATION_STARTING_POINT:
      // Sent by ChromeVox but doesn't need to be handled by aura.
      break;
    case ui::AX_ACTION_BLUR:
    case ui::AX_ACTION_DECREMENT:
    case ui::AX_ACTION_GET_IMAGE_DATA:
    case ui::AX_ACTION_HIT_TEST:
    case ui::AX_ACTION_INCREMENT:
    case ui::AX_ACTION_REPLACE_SELECTED_TEXT:
    case ui::AX_ACTION_SCROLL_TO_POINT:
    case ui::AX_ACTION_SET_SCROLL_OFFSET:
    case ui::AX_ACTION_SET_VALUE:
      // Not implemented yet.
      NOTREACHED();
      break;
    case ui::AX_ACTION_NONE:
      NOTREACHED();
      break;
  }
}

void AutomationManagerAura::OnChildWindowRemoved(
    views::AXAuraObjWrapper* parent) {
  if (!enabled_)
    return;

  if (!parent)
    parent = current_tree_->GetRoot();

  SendEvent(nullptr, parent, ui::AX_EVENT_CHILDREN_CHANGED);
}

AutomationManagerAura::AutomationManagerAura()
    : enabled_(false), processing_events_(false) {}

AutomationManagerAura::~AutomationManagerAura() {
}

void AutomationManagerAura::ResetSerializer() {
  current_tree_serializer_.reset(
      new AuraAXTreeSerializer(current_tree_.get()));
}

void AutomationManagerAura::SendEvent(BrowserContext* context,
                                      views::AXAuraObjWrapper* aura_obj,
                                      ui::AXEvent event_type) {
  if (!context && g_browser_process->profile_manager()) {
    context = g_browser_process->profile_manager()->GetLastUsedProfile();
  }

  if (!context) {
    LOG(WARNING) << "Accessibility notification but no browser context";
    return;
  }

  if (processing_events_) {
    pending_events_.push_back(std::make_pair(aura_obj, event_type));
    return;
  }
  processing_events_ = true;

  ExtensionMsg_AccessibilityEventParams params;
  if (!current_tree_serializer_->SerializeChanges(aura_obj, &params.update)) {
    LOG(ERROR) << "Unable to serialize one accessibility event.";
    return;
  }

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus =
      views::AXAuraObjCache::GetInstance()->GetFocus();
  if (focus)
    current_tree_serializer_->SerializeChanges(focus, &params.update);

  params.tree_id = 0;
  params.id = aura_obj->GetID();
  params.event_type = event_type;
  params.mouse_location = aura::Env::GetInstance()->last_mouse_location();
  AutomationEventRouter* router = AutomationEventRouter::GetInstance();
  router->DispatchAccessibilityEvent(params);

  processing_events_ = false;
  auto pending_events_copy = pending_events_;
  pending_events_.clear();
  for (size_t i = 0; i < pending_events_copy.size(); ++i) {
    SendEvent(context,
              pending_events_copy[i].first,
              pending_events_copy[i].second);
  }
}
