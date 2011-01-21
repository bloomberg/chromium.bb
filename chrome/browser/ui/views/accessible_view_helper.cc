// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessible_view_helper.h"

#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/accessibility_event_router_views.h"
#include "chrome/common/notification_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/widget.h"
#include "views/view.h"

AccessibleViewHelper::AccessibleViewHelper(
    views::View* view_tree, Profile* profile)
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
         iter != managed_views_.end();
         ++iter) {
      accessibility_event_router_->RemoveView(*iter);
    }
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

void AccessibleViewHelper::SetViewName(views::View* view, std::string name) {
  if (!view_tree_)
    return;

  accessibility_event_router_->SetViewName(view, name);
  managed_views_.push_back(view);

 #if defined(OS_LINUX)
  gfx::NativeView native_view = GetNativeView(view);
  if (native_view)
    widget_helper_->SetWidgetName(native_view, name);
 #endif
}

void AccessibleViewHelper::SetViewName(views::View* view, int string_id) {
  const std::string name = l10n_util::GetStringUTF8(string_id);
  SetViewName(view, name);
}

gfx::NativeView AccessibleViewHelper::GetNativeView(views::View* view) const {
  if (view->GetClassName() == views::NativeViewHost::kViewClassName)
    return static_cast<views::NativeViewHost*>(view)->native_view();

  for (int i = 0; i < view->GetChildViewCount(); i++) {
    gfx::NativeView native_view = GetNativeView(view->GetChildViewAt(i));
    if (native_view)
      return native_view;
  }

  return NULL;
}
