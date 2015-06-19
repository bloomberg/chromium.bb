// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <vector>

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using content::BrowserContext;
using extensions::AutomationEventRouter;

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  return Singleton<AutomationManagerAura>::get();
}

void AutomationManagerAura::Enable(BrowserContext* context) {
  enabled_ = true;
  if (!current_tree_.get())
    current_tree_.reset(new AXTreeSourceAura());
  ResetSerializer();
  SendEvent(context, current_tree_->GetRoot(), ui::AX_EVENT_LOAD_COMPLETE);
  if (!pending_alert_text_.empty()) {
    HandleAlert(context, pending_alert_text_);
    pending_alert_text_.clear();
  }
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

  if (!context && g_browser_process->profile_manager())
    context = g_browser_process->profile_manager()->GetLastUsedProfile();

  if (!context) {
    LOG(WARNING) << "Accessibility notification but no browser context";
    return;
  }

  views::AXAuraObjWrapper* aura_obj =
      views::AXAuraObjCache::GetInstance()->GetOrCreate(view);

  if (processing_events_) {
    pending_events_.push_back(std::make_pair(aura_obj, event_type));
    return;
  }

  processing_events_ = true;
  SendEvent(context, aura_obj, event_type);

  for (size_t i = 0; i < pending_events_.size(); ++i)
    SendEvent(context, pending_events_[i].first, pending_events_[i].second);

  processing_events_ = false;
  pending_events_.clear();
}

void AutomationManagerAura::HandleAlert(content::BrowserContext* context,
                                        const std::string& text) {
  if (!enabled_) {
    pending_alert_text_ = text;
    return;
  }

  views::AXAuraObjWrapper* obj =
      static_cast<AXRootObjWrapper*>(current_tree_->GetRoot())
          ->GetAlertForText(text);
  SendEvent(context, obj, ui::AX_EVENT_ALERT);
}

void AutomationManagerAura::DoDefault(int32 id) {
  CHECK(enabled_);
  current_tree_->DoDefault(id);
}

void AutomationManagerAura::Focus(int32 id) {
  CHECK(enabled_);
  current_tree_->Focus(id);
}

void AutomationManagerAura::MakeVisible(int32 id) {
  CHECK(enabled_);
  current_tree_->MakeVisible(id);
}

void AutomationManagerAura::SetSelection(int32 id, int32 start, int32 end) {
  CHECK(enabled_);
  current_tree_->SetSelection(id, start, end);
}

void AutomationManagerAura::ShowContextMenu(int32 id) {
  CHECK(enabled_);
  current_tree_->ShowContextMenu(id);
}

AutomationManagerAura::AutomationManagerAura()
    : enabled_(false), processing_events_(false) {
}

AutomationManagerAura::~AutomationManagerAura() {
}

void AutomationManagerAura::ResetSerializer() {
  current_tree_serializer_.reset(
      new ui::AXTreeSerializer<views::AXAuraObjWrapper*>(current_tree_.get()));
}

void AutomationManagerAura::SendEvent(BrowserContext* context,
                                      views::AXAuraObjWrapper* aura_obj,
                                      ui::AXEvent event_type) {
  ExtensionMsg_AccessibilityEventParams params;
  current_tree_serializer_->SerializeChanges(aura_obj, &params.update);
  params.tree_id = 0;
  params.id = aura_obj->GetID();
  params.event_type = event_type;
  AutomationEventRouter* router = AutomationEventRouter::GetInstance();
  router->DispatchAccessibilityEvent(params);
}
