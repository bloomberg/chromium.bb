// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/accessibility_event_router_views.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/views/location_bar/location_bar_view.h"
#include "chrome/common/notification_type.h"
#include "views/accessibility/accessibility_types.h"
#include "views/controls/button/custom_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/link.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"
#include "views/view.h"

#if defined(OS_WIN)
#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#endif

using views::FocusManager;

AccessibilityEventRouterViews::AccessibilityEventRouterViews()
    : most_recent_profile_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

AccessibilityEventRouterViews::~AccessibilityEventRouterViews() {
}

// static
AccessibilityEventRouterViews* AccessibilityEventRouterViews::GetInstance() {
  return Singleton<AccessibilityEventRouterViews>::get();
}

bool AccessibilityEventRouterViews::AddViewTree(
    views::View* view, Profile* profile) {
  if (view_tree_profile_map_[view] != NULL)
    return false;

  view_tree_profile_map_[view] = profile;
  return true;
}

void AccessibilityEventRouterViews::RemoveViewTree(views::View* view) {
  DCHECK(view_tree_profile_map_.find(view) !=
         view_tree_profile_map_.end());
  view_tree_profile_map_.erase(view);
}

void AccessibilityEventRouterViews::IgnoreView(views::View* view) {
  view_info_map_[view].ignore = true;
}

void AccessibilityEventRouterViews::SetViewName(
    views::View* view, std::string name) {
  view_info_map_[view].name = name;
}

void AccessibilityEventRouterViews::RemoveView(views::View* view) {
  DCHECK(view_info_map_.find(view) != view_info_map_.end());
  view_info_map_.erase(view);
}

void AccessibilityEventRouterViews::HandleAccessibilityEvent(
    views::View* view, AccessibilityTypes::Event event_type) {
  if (!ExtensionAccessibilityEventRouter::GetInstance()->
      IsAccessibilityEnabled()) {
    return;
  }

  switch (event_type) {
    case AccessibilityTypes::EVENT_FOCUS:
      DispatchAccessibilityNotification(
          view, NotificationType::ACCESSIBILITY_CONTROL_FOCUSED);
      break;
    case AccessibilityTypes::EVENT_MENUSTART:
    case AccessibilityTypes::EVENT_MENUPOPUPSTART:
      DispatchAccessibilityNotification(
          view, NotificationType::ACCESSIBILITY_MENU_OPENED);
      break;
    case AccessibilityTypes::EVENT_MENUEND:
    case AccessibilityTypes::EVENT_MENUPOPUPEND:
      DispatchAccessibilityNotification(
          view, NotificationType::ACCESSIBILITY_MENU_CLOSED);
      break;
    case AccessibilityTypes::EVENT_TEXT_CHANGED:
    case AccessibilityTypes::EVENT_SELECTION_CHANGED:
      DispatchAccessibilityNotification(
          view, NotificationType::ACCESSIBILITY_TEXT_CHANGED);
      break;
    case AccessibilityTypes::EVENT_VALUE_CHANGED:
      DispatchAccessibilityNotification(
          view, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
      break;
    case AccessibilityTypes::EVENT_ALERT:
    case AccessibilityTypes::EVENT_NAME_CHANGED:
      // TODO(dmazzoni): re-evaluate this list later and see
      // if supporting any of these would be useful feature requests or
      // they'd just be superfluous.
      NOTIMPLEMENTED();
      break;
  }
}

//
// Private methods
//

void AccessibilityEventRouterViews::FindView(
    views::View* view, Profile** profile, bool* is_accessible) {
  *is_accessible = false;

  // First see if it's a descendant of an accessible view.
  for (base::hash_map<views::View*, Profile*>::const_iterator iter =
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
  base::hash_map<views::View*, ViewInfo>::const_iterator iter =
      view_info_map_.find(view);
  if (iter != view_info_map_.end() && iter->second.ignore)
    *is_accessible = false;
}

std::string AccessibilityEventRouterViews::GetViewName(views::View* view) {
  std::string name;

  // First see if we have a name registered for this view.
  base::hash_map<views::View*, ViewInfo>::const_iterator iter =
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
    views::View* view, NotificationType type) {
  Profile* profile = NULL;
  bool is_accessible;
  FindView(view, &profile, &is_accessible);

  // Special case: a menu isn't associated with any particular top-level
  // window, so menu events get routed to the profile of the most recent
  // event that was associated with a window, which should be the window
  // that triggered opening the menu.
  bool is_menu_event = IsMenuEvent(view, type);
  if (is_menu_event && !profile && most_recent_profile_) {
    profile = most_recent_profile_;
    is_accessible = true;
  }

  if (!is_accessible)
    return;

  most_recent_profile_ = profile;

  std::string class_name = view->GetClassName();

  if (class_name == views::MenuButton::kViewClassName ||
      type == NotificationType::ACCESSIBILITY_MENU_OPENED ||
      type == NotificationType::ACCESSIBILITY_MENU_CLOSED) {
    SendMenuNotification(view, type, profile);
  } else if (is_menu_event) {
    SendMenuItemNotification(view, type, profile);
  } else if (class_name == views::CustomButton::kViewClassName ||
             class_name == views::NativeButton::kViewClassName ||
             class_name == views::TextButton::kViewClassName) {
    SendButtonNotification(view, type, profile);
  } else if (class_name == views::Link::kViewClassName) {
    SendLinkNotification(view, type, profile);
  } else if (class_name == LocationBarView::kViewClassName) {
    SendLocationBarNotification(view, type, profile);
  } else {
    class_name += " ";
  }
}

void AccessibilityEventRouterViews::SendButtonNotification(
    views::View* view, NotificationType type, Profile* profile) {
  AccessibilityButtonInfo info(profile, GetViewName(view));
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterViews::SendLinkNotification(
    views::View* view, NotificationType type, Profile* profile) {
  AccessibilityLinkInfo info(profile, GetViewName(view));
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterViews::SendMenuNotification(
    views::View* view, NotificationType type, Profile* profile) {
  AccessibilityMenuInfo info(profile, GetViewName(view));
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterViews::SendMenuItemNotification(
    views::View* view, NotificationType type, Profile* profile) {
  std::string name = GetViewName(view);

  bool has_submenu = false;
  int index = -1;
  int count = -1;

  if (view->GetClassName() == views::MenuItemView::kViewClassName)
    has_submenu = static_cast<views::MenuItemView*>(view)->HasSubmenu();

  views::View* parent_menu = view->GetParent();
  while (parent_menu != NULL && parent_menu->GetClassName() !=
         views::SubmenuView::kViewClassName) {
    parent_menu = parent_menu->GetParent();
  }
  if (parent_menu) {
    count = 0;
    RecursiveGetMenuItemIndexAndCount(parent_menu, view, &index, &count);
  }

  AccessibilityMenuItemInfo info(profile, name, has_submenu, index, count);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterViews::RecursiveGetMenuItemIndexAndCount(
    views::View* menu, views::View* item, int* index, int* count) {
  for (int i = 0; i < menu->GetChildViewCount(); ++i) {
    views::View* child = menu->GetChildViewAt(i);
    int previous_count = *count;
    RecursiveGetMenuItemIndexAndCount(child, item, index, count);
    if (child->GetClassName() == views::MenuItemView::kViewClassName &&
        *count == previous_count) {
      if (item == child)
        *index = *count;
      (*count)++;
    } else if (child->GetClassName() == views::TextButton::kViewClassName) {
      if (item == child)
        *index = *count;
      (*count)++;
    }
  }
}

bool AccessibilityEventRouterViews::IsMenuEvent(
    views::View* view, NotificationType type) {
  if (type == NotificationType::ACCESSIBILITY_MENU_OPENED ||
      type == NotificationType::ACCESSIBILITY_MENU_CLOSED)
    return true;

  while (view) {
    AccessibilityTypes::Role role = view->GetAccessibleRole();
    if (role == AccessibilityTypes::ROLE_MENUITEM ||
        role == AccessibilityTypes::ROLE_MENUPOPUP) {
      return true;
    }
    view = view->GetParent();
  }

  return false;
}

void AccessibilityEventRouterViews::SendLocationBarNotification(
    views::View* view, NotificationType type, Profile* profile) {
#if defined(OS_WIN)
  // This particular method isn't needed on Linux/Views, we get text
  // notifications directly from GTK.
  std::string name = GetViewName(view);
  LocationBarView* location_bar = static_cast<LocationBarView*>(view);
  AutocompleteEditViewWin* location_entry =
      static_cast<AutocompleteEditViewWin*>(location_bar->location_entry());

  std::string value = WideToUTF8(location_entry->GetText());
  std::wstring::size_type selection_start;
  std::wstring::size_type selection_end;
  location_entry->GetSelectionBounds(&selection_start, &selection_end);

  AccessibilityTextBoxInfo info(profile, name, false);
  info.SetValue(value, selection_start, selection_end);
  SendAccessibilityNotification(type, &info);
#endif
}
