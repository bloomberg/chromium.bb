// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/automation_manager_ash.h"

#include <vector>

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/automation_internal/automation_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_context.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using content::BrowserContext;

// static
AutomationManagerAsh* AutomationManagerAsh::GetInstance() {
  return Singleton<AutomationManagerAsh>::get();
}

void AutomationManagerAsh::Enable(BrowserContext* context) {
  enabled_ = true;
  Reset();
  SendEvent(context, current_tree_->GetRoot(), ui::AX_EVENT_LOAD_COMPLETE);
}

void AutomationManagerAsh::Disable() {
  enabled_ = false;

  // Reset the serializer to save memory.
  current_tree_serializer_->Reset();
}

void AutomationManagerAsh::HandleEvent(BrowserContext* context,
                                         views::View* view,
                                         ui::AXEvent event_type) {
  if (!enabled_) {
    return;
  }

  // TODO(dtseng): Events should only be delivered to extensions with the
  // desktop permission.
  views::Widget* widget = view->GetWidget();
  if (!widget)
    return;

  if (!context && g_browser_process->profile_manager()) {
    context = g_browser_process->profile_manager()->GetLastUsedProfile();
  }
  if (!context) {
    LOG(WARNING) << "Accessibility notification but no browser context";
    return;
  }

  views::AXAuraObjWrapper* aura_obj =
      views::AXAuraObjCache::GetInstance()->GetOrCreate(view);
  SendEvent(context, aura_obj, event_type);
}

void AutomationManagerAsh::DoDefault(int32 id) {
  CHECK(enabled_);
  current_tree_->DoDefault(id);
}

void AutomationManagerAsh::Focus(int32 id) {
  CHECK(enabled_);
  current_tree_->Focus(id);
}

void AutomationManagerAsh::MakeVisible(int32 id) {
  CHECK(enabled_);
  current_tree_->MakeVisible(id);
}

void AutomationManagerAsh::SetSelection(int32 id, int32 start, int32 end) {
  CHECK(enabled_);
  current_tree_->SetSelection(id, start, end);
}

AutomationManagerAsh::AutomationManagerAsh() : enabled_(false) {}

AutomationManagerAsh::~AutomationManagerAsh() {}

void AutomationManagerAsh::Reset() {
  current_tree_.reset(new AXTreeSourceAsh());
  current_tree_serializer_.reset(
      new ui::AXTreeSerializer<views::AXAuraObjWrapper*>(
          current_tree_.get()));
}

void AutomationManagerAsh::SendEvent(BrowserContext* context,
                                       views::AXAuraObjWrapper* aura_obj,
                                       ui::AXEvent event_type) {
  ui::AXTreeUpdate update;
  current_tree_serializer_->SerializeChanges(aura_obj, &update);

  // Route this event to special process/routing ids recognized by the
  // Automation API as the desktop tree.
  // TODO(dtseng): Would idealy define these special desktop constants in idl.
  content::AXEventNotificationDetails detail(update.node_id_to_clear,
                                             update.nodes,
                                             event_type,
                                             aura_obj->GetID(),
                                             0, /* process_id */
                                             0 /* routing_id */);
  std::vector<content::AXEventNotificationDetails> details;
  details.push_back(detail);
  extensions::automation_util::DispatchAccessibilityEventsToAutomation(
      details, context, gfx::Vector2d());
}
