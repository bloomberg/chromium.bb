// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_accessibility_delegate.h"

#include <limits>

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "ui/aura/env.h"

using chromeos::AccessibilityManager;
using chromeos::MagnificationManager;

ChromeAccessibilityDelegate::ChromeAccessibilityDelegate() = default;

ChromeAccessibilityDelegate::~ChromeAccessibilityDelegate() = default;

void ChromeAccessibilityDelegate::SetMagnifierEnabled(bool enabled) {
  DCHECK(MagnificationManager::Get());
  return MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

bool ChromeAccessibilityDelegate::IsMagnifierEnabled() const {
  DCHECK(MagnificationManager::Get());
  return MagnificationManager::Get()->IsMagnifierEnabled();
}

bool ChromeAccessibilityDelegate::ShouldShowAccessibilityMenu() const {
  DCHECK(AccessibilityManager::Get());
  return AccessibilityManager::Get()->ShouldShowAccessibilityMenu();
}

void ChromeAccessibilityDelegate::SaveScreenMagnifierScale(double scale) {
  if (MagnificationManager::Get())
    MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
}

double ChromeAccessibilityDelegate::GetSavedScreenMagnifierScale() {
  if (MagnificationManager::Get())
    return MagnificationManager::Get()->GetSavedScreenMagnifierScale();

  return std::numeric_limits<double>::min();
}

void ChromeAccessibilityDelegate::DispatchAccessibilityEvent(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const ui::AXEvent& event) {
  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = tree_id;
  for (const ui::AXTreeUpdate& update : updates)
    event_bundle.updates.push_back(update);
  event_bundle.events.push_back(event);
  event_bundle.mouse_location = aura::Env::GetInstance()->last_mouse_location();

  // Forward the tree updates and the event to the accessibility extension.
  extensions::AutomationEventRouter::GetInstance()->DispatchAccessibilityEvents(
      event_bundle);
}

void ChromeAccessibilityDelegate::DispatchTreeDestroyedEvent(
    const ui::AXTreeID& tree_id) {
  extensions::AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
      tree_id, nullptr /* browser_context */);
}
