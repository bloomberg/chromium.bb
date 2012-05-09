// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_gtk.h"

#include "content/browser/accessibility/browser_accessibility_gtk.h"
#include "content/common/accessibility_messages.h"

using webkit_glue::WebAccessibility;

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    gfx::NativeView parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerGtk(
      parent_view,
      src,
      delegate,
      factory);
}

BrowserAccessibilityManagerGtk::BrowserAccessibilityManagerGtk(
    GtkWidget* parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(parent_view, src, delegate, factory) {
}

BrowserAccessibilityManagerGtk::~BrowserAccessibilityManagerGtk() {
}

void BrowserAccessibilityManagerGtk::NotifyAccessibilityEvent(
    int type,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;
  AtkObject* atk_object = node->ToBrowserAccessibilityGtk()->GetAtkObject();

  switch (type) {
    case AccessibilityNotificationChildrenChanged:
      RecursivelySendChildrenChanged(GetRoot()->ToBrowserAccessibilityGtk());
      break;
    case AccessibilityNotificationFocusChanged:
      // Note: atk_focus_tracker_notify may be deprecated in the future;
      // follow this bug for the replacement:
      // https://bugzilla.gnome.org/show_bug.cgi?id=649575#c4
      g_signal_emit_by_name(atk_object, "focus-event", true);
      atk_focus_tracker_notify(atk_object);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerGtk::RecursivelySendChildrenChanged(
    BrowserAccessibilityGtk* node) {
  AtkObject* atkObject = node->ToBrowserAccessibilityGtk()->GetAtkObject();
  for (unsigned int i = 0; i < node->children().size(); ++i) {
    BrowserAccessibilityGtk* child =
        node->children()[i]->ToBrowserAccessibilityGtk();
    g_signal_emit_by_name(atkObject,
                          "children-changed::add",
                          i,
                          child->GetAtkObject());
    RecursivelySendChildrenChanged(child);
  }
}
