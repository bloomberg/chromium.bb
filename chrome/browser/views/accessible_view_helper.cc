// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/accessible_view_helper.h"

#include "app/l10n_util.h"
#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "views/widget/widget.h"

using views::View;

AccessibleViewHelper::AccessibleViewHelper(
    View* view_tree, Profile* profile)
    : accessibility_event_router_(AccessibilityEventRouterViews::GetInstance()),
      profile_(profile),
      view_tree_(view_tree) {
  if (!accessibility_event_router_->AddViewTree(view_tree_, profile))
    view_tree_ = NULL;

#if defined(OS_LINUX)
  GtkWidget* widget = view_tree->GetWidget()->GetNativeView();
  widget_helper_.reset(new AccessibleWidgetHelper(widget, profile));
#endif
}

AccessibleViewHelper::~AccessibleViewHelper() {
  if (!window_title_.empty()) {
    AccessibilityWindowInfo info(profile_, window_title_);
    NotificationService::current()->Notify(
        NotificationType::ACCESSIBILITY_WINDOW_CLOSED,
        Source<Profile>(profile_),
        Details<AccessibilityWindowInfo>(&info));
  }

  if (view_tree_) {
    accessibility_event_router_->RemoveViewTree(view_tree_);
    for (std::vector<views::View*>::iterator iter = managed_views_.begin();
         iter != managed_views_.end(); ++iter)
      accessibility_event_router_->RemoveView(*iter);
  }
}

void AccessibleViewHelper::SendOpenWindowNotification(
    const std::string& window_title) {
  window_title_ = window_title;
  AccessibilityWindowInfo info(profile_, window_title);
  NotificationService::current()->Notify(
      NotificationType::ACCESSIBILITY_WINDOW_OPENED,
      Source<Profile>(profile_),
      Details<AccessibilityWindowInfo>(&info));
}

void AccessibleViewHelper::IgnoreView(View* view) {
  if (!view_tree_)
    return;

  accessibility_event_router_->IgnoreView(view);
  managed_views_.push_back(view);
}

void AccessibleViewHelper::SetViewName(View* view, std::string name) {
  if (!view_tree_)
    return;

  accessibility_event_router_->SetViewName(view, name);
  managed_views_.push_back(view);
}

void AccessibleViewHelper::SetViewName(View* view, int string_id) {
  if (!view_tree_)
    return;

  std::string name = l10n_util::GetStringUTF8(string_id);
  accessibility_event_router_->SetViewName(view, name);
  managed_views_.push_back(view);
}
