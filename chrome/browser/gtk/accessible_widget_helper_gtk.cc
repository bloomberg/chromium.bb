// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/accessible_widget_helper_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"

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
  for (unsigned int i = 0; i < managed_widgets_.size(); i++) {
    accessibility_event_router_->RemoveWidget(managed_widgets_[i]);
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

void AccessibleWidgetHelper::IgnoreWidget(GtkWidget* widget) {
  accessibility_event_router_->IgnoreWidget(widget);
  managed_widgets_.push_back(widget);
}

void AccessibleWidgetHelper::SetWidgetName(
    GtkWidget* widget, std::string name) {
  accessibility_event_router_->SetWidgetName(widget, name);
  managed_widgets_.push_back(widget);
}

void AccessibleWidgetHelper::SetWidgetName(
    GtkWidget* widget, int string_id) {
  std::string name = l10n_util::GetStringUTF8(string_id);
  accessibility_event_router_->SetWidgetName(widget, name);
  managed_widgets_.push_back(widget);
}
