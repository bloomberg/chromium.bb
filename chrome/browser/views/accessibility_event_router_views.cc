// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/accessibility_event_router_views.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_type.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/link.h"

using views::FocusManager;
using views::View;

AccessibilityEventRouterViews::AccessibilityEventRouterViews()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

AccessibilityEventRouterViews::~AccessibilityEventRouterViews() {
}

// static
AccessibilityEventRouterViews* AccessibilityEventRouterViews::GetInstance() {
  return Singleton<AccessibilityEventRouterViews>::get();
}

bool AccessibilityEventRouterViews::AddViewTree(View* view, Profile* profile) {
  if (view_tree_profile_map_[view] != NULL)
    return false;

  view_tree_profile_map_[view] = profile;
  FocusManager* focus_manager = view->GetFocusManager();

  // Add this object as a listener for this focus manager, but use a ref
  // count to ensure we only call AddFocusChangeListener on any given
  // focus manager once. Note that hash_map<FocusManager*, int>::operator[]
  // will initialize the ref count to zero if it's not already in the map.
  if (focus_manager_ref_count_[focus_manager] == 0) {
    focus_manager->AddFocusChangeListener(this);
  }
  focus_manager_ref_count_[focus_manager]++;

  view_info_map_[view].focus_manager = focus_manager;
  return true;
}

void AccessibilityEventRouterViews::RemoveViewTree(View* view) {
  DCHECK(view_tree_profile_map_.find(view) !=
         view_tree_profile_map_.end());
  view_tree_profile_map_.erase(view);

  // Decrement the ref count of the focus manager, and remove this object
  // as a listener if the count reaches zero.
  FocusManager* focus_manager = view_info_map_[view].focus_manager;
  DCHECK(focus_manager);
  focus_manager_ref_count_[focus_manager]--;
  if (focus_manager_ref_count_[focus_manager] == 0) {
    focus_manager->RemoveFocusChangeListener(this);
  }
}

void AccessibilityEventRouterViews::IgnoreView(View* view) {
  view_info_map_[view].ignore = true;
}

void AccessibilityEventRouterViews::SetViewName(View* view, std::string name) {
  view_info_map_[view].name = name;
}

void AccessibilityEventRouterViews::RemoveView(View* view) {
  DCHECK(view_info_map_.find(view) != view_info_map_.end());
  view_info_map_.erase(view);
}

//
// views::FocusChangeListener
//

void AccessibilityEventRouterViews::FocusWillChange(
    View* focused_before, View* focused_now) {
  if (focused_now) {
    DispatchAccessibilityNotification(
        focused_now, NotificationType::ACCESSIBILITY_CONTROL_FOCUSED);
  }
}

//
// Private methods
//

void AccessibilityEventRouterViews::FindView(
    View* view, Profile** profile, bool* is_accessible) {
  *is_accessible = false;

  // First see if it's a descendant of an accessible view.
  for (base::hash_map<View*, Profile*>::const_iterator iter =
           view_tree_profile_map_.begin();
       iter != view_tree_profile_map_.end();
       ++iter) {
    if (iter->first->IsParentOf(view)) {
      *is_accessible = true;
      if (profile)
        *profile = iter->second;
      break;
    }
  }
  if (!*is_accessible)
    return;

  // Now make sure it's not marked as a widget to be ignored.
  base::hash_map<View*, ViewInfo>::const_iterator iter =
      view_info_map_.find(view);
  if (iter != view_info_map_.end() && iter->second.ignore)
    *is_accessible = false;
}

std::string AccessibilityEventRouterViews::GetViewName(View* view) {
  std::string name;

  // First see if we have a name registered for this view.
  base::hash_map<View*, ViewInfo>::const_iterator iter =
      view_info_map_.find(view);
  if (iter != view_info_map_.end())
    name = iter->second.name;

  // Otherwise ask the view for its accessible name.
  if (name.empty()) {
    std::wstring wname;
    view->GetAccessibleName(&wname);
    name = WideToUTF8(wname);
  }

  return name;
}

void AccessibilityEventRouterViews::DispatchAccessibilityNotification(
    View* view, NotificationType type) {
  Profile* profile = NULL;
  bool is_accessible;
  FindView(view, &profile, &is_accessible);
  if (!is_accessible)
    return;

  if (view->GetClassName() == views::ImageButton::kViewClassName) {
    SendButtonNotification(view, type, profile);
  } else if (view->GetClassName() == views::NativeButton::kViewClassName) {
    SendButtonNotification(view, type, profile);
  } else if (view->GetClassName() == views::Link::kViewClassName) {
    SendLinkNotification(view, type, profile);
  } else if (view->GetClassName() == views::MenuButton::kViewClassName) {
    SendMenuNotification(view, type, profile);
  }
}

void AccessibilityEventRouterViews::SendButtonNotification(
    View* view, NotificationType type, Profile* profile) {
  AccessibilityButtonInfo info(profile, GetViewName(view));
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterViews::SendLinkNotification(
    View* view, NotificationType type, Profile* profile) {
  AccessibilityLinkInfo info(profile, GetViewName(view));
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterViews::SendMenuNotification(
    View* view, NotificationType type, Profile* profile) {
  AccessibilityMenuInfo info(profile, GetViewName(view));
  SendAccessibilityNotification(type, &info);
}
