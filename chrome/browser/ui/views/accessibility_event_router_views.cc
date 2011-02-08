// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility_event_router_views.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/notification_type.h"
#include "views/accessibility/accessibility_types.h"
#include "views/controls/button/custom_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/link.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"
#include "views/view.h"
#include "views/window/window.h"

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

std::string AccessibilityEventRouterViews::GetViewName(views::View* view) {
  string16 wname;
  view->GetAccessibleName(&wname);
  return UTF16ToUTF8(wname);
}

void AccessibilityEventRouterViews::DispatchAccessibilityNotification(
    views::View* view, NotificationType type) {
  // Get the profile associated with this view. If it's not found, use
  // the most recent profile where accessibility events were sent, or
  // the default profile.
  Profile* profile = NULL;
  views::Window* window = view->GetWindow();
  if (window) {
    profile = reinterpret_cast<Profile*>(window->GetNativeWindowProperty(
        Profile::kProfileKey));
  }
  if (!profile)
    profile = most_recent_profile_;
  if (!profile)
    profile = g_browser_process->profile_manager()->GetDefaultProfile();
  if (!profile) {
    NOTREACHED();
    return;
  }

  most_recent_profile_ = profile;

  std::string class_name = view->GetClassName();
  if (class_name == views::MenuButton::kViewClassName ||
      type == NotificationType::ACCESSIBILITY_MENU_OPENED ||
      type == NotificationType::ACCESSIBILITY_MENU_CLOSED) {
    SendMenuNotification(view, type, profile);
  } else if (IsMenuEvent(view, type)) {
    SendMenuItemNotification(view, type, profile);
  } else if (class_name == views::CustomButton::kViewClassName ||
             class_name == views::NativeButton::kViewClassName ||
             class_name == views::TextButton::kViewClassName) {
    SendButtonNotification(view, type, profile);
  } else if (class_name == views::Link::kViewClassName) {
    SendLinkNotification(view, type, profile);
  } else if (class_name == LocationBarView::kViewClassName) {
    SendLocationBarNotification(view, type, profile);
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
