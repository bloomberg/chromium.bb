// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/accessible_widget_helper_gtk.h"

#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/notification_service.h"
#include "ui/base/l10n/l10n_util.h"

AccessibleWidgetHelper::AccessibleWidgetHelper(
    GtkWidget* root_widget, Profile* profile)
    : accessibility_event_router_(AccessibilityEventRouterGtk::GetInstance()),
      profile_(profile),
      root_widget_(root_widget) {
  CHECK(profile_);
  accessibility_event_router_->AddRootWidget(root_widget_, profile_);
}

AccessibleWidgetHelper::~AccessibleWidgetHelper() {
  if (!window_title_.empty()) {
    AccessibilityWindowInfo info(profile_, window_title_);
    NotificationService::current()->Notify(
        NotificationType::ACCESSIBILITY_WINDOW_CLOSED,
        Source<Profile>(profile_),
        Details<AccessibilityWindowInfo>(&info));
  }

  if (root_widget_)
    accessibility_event_router_->RemoveRootWidget(root_widget_);
  for (std::set<GtkWidget*>::iterator it = managed_widgets_.begin();
       it != managed_widgets_.end();
       ++it) {
    accessibility_event_router_->RemoveWidgetNameOverride(*it);
  }
}

void AccessibleWidgetHelper::SendOpenWindowNotification(
    const std::string& window_title) {
  window_title_ = window_title;
  AccessibilityWindowInfo info(profile_, window_title);
  NotificationService::current()->Notify(
      NotificationType::ACCESSIBILITY_WINDOW_OPENED,
      Source<Profile>(profile_),
      Details<AccessibilityWindowInfo>(&info));
}

void AccessibleWidgetHelper::SetWidgetName(
    GtkWidget* widget, std::string name) {
  if (managed_widgets_.find(widget) != managed_widgets_.end()) {
    // AccessibilityEventRouterGtk reference-counts its widgets, but we
    // don't. In order to avoid a memory leak, tell the event router
    // to deref first, so the resulting refcount is unchanged after we
    // call SetWidgetName.
    accessibility_event_router_->RemoveWidgetNameOverride(widget);
  }

  accessibility_event_router_->AddWidgetNameOverride(widget, name);
  managed_widgets_.insert(widget);
}

void AccessibleWidgetHelper::SetWidgetName(
    GtkWidget* widget, int string_id) {
  SetWidgetName(widget, l10n_util::GetStringUTF8(string_id));
}
