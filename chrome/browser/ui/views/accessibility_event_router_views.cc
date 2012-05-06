// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility_event_router_views.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using views::FocusManager;

AccessibilityEventRouterViews::AccessibilityEventRouterViews()
    : most_recent_profile_(NULL) {
}

AccessibilityEventRouterViews::~AccessibilityEventRouterViews() {
}

// static
AccessibilityEventRouterViews* AccessibilityEventRouterViews::GetInstance() {
  return Singleton<AccessibilityEventRouterViews>::get();
}

void AccessibilityEventRouterViews::HandleAccessibilityEvent(
    views::View* view, ui::AccessibilityTypes::Event event_type) {
  if (!ExtensionAccessibilityEventRouter::GetInstance()->
      IsAccessibilityEnabled()) {
    return;
  }

  switch (event_type) {
    case ui::AccessibilityTypes::EVENT_FOCUS:
      DispatchAccessibilityNotification(
          view, chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED);
      break;
    case ui::AccessibilityTypes::EVENT_MENUSTART:
    case ui::AccessibilityTypes::EVENT_MENUPOPUPSTART:
      DispatchAccessibilityNotification(
          view, chrome::NOTIFICATION_ACCESSIBILITY_MENU_OPENED);
      break;
    case ui::AccessibilityTypes::EVENT_MENUEND:
    case ui::AccessibilityTypes::EVENT_MENUPOPUPEND:
      DispatchAccessibilityNotification(
          view, chrome::NOTIFICATION_ACCESSIBILITY_MENU_CLOSED);
      break;
    case ui::AccessibilityTypes::EVENT_TEXT_CHANGED:
    case ui::AccessibilityTypes::EVENT_SELECTION_CHANGED:
      DispatchAccessibilityNotification(
          view, chrome::NOTIFICATION_ACCESSIBILITY_TEXT_CHANGED);
      break;
    case ui::AccessibilityTypes::EVENT_VALUE_CHANGED:
      DispatchAccessibilityNotification(
          view, chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_ACTION);
      break;
    case ui::AccessibilityTypes::EVENT_ALERT:
      DispatchAccessibilityNotification(
          view, chrome::NOTIFICATION_ACCESSIBILITY_WINDOW_OPENED);
      break;
    case ui::AccessibilityTypes::EVENT_NAME_CHANGED:
      NOTIMPLEMENTED();
      break;
  }
}

void AccessibilityEventRouterViews::HandleMenuItemFocused(
    const string16& menu_name,
    const string16& menu_item_name,
    int item_index,
    int item_count,
    bool has_submenu) {
  if (!ExtensionAccessibilityEventRouter::GetInstance()->
      IsAccessibilityEnabled()) {
    return;
  }

  if (!most_recent_profile_)
    return;

  AccessibilityMenuItemInfo info(most_recent_profile_,
                                 UTF16ToUTF8(menu_item_name),
                                 UTF16ToUTF8(menu_name),
                                 has_submenu,
                                 item_index,
                                 item_count);
  SendAccessibilityNotification(
      chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED, &info);
}

//
// Private methods
//

void AccessibilityEventRouterViews::DispatchAccessibilityNotification(
    views::View* view, int type) {
  // Get the profile associated with this view. If it's not found, use
  // the most recent profile where accessibility events were sent, or
  // the default profile.
  Profile* profile = NULL;
  views::Widget* widget = view->GetWidget();
  if (widget) {
    profile = reinterpret_cast<Profile*>(
        widget->GetNativeWindowProperty(Profile::kProfileKey));
  }
  if (!profile)
    profile = most_recent_profile_;
  if (!profile)
    profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  if (!profile) {
    NOTREACHED();
    return;
  }

  most_recent_profile_ = profile;

  if (type == chrome::NOTIFICATION_ACCESSIBILITY_MENU_OPENED ||
      type == chrome::NOTIFICATION_ACCESSIBILITY_MENU_CLOSED) {
    SendMenuNotification(view, type, profile);
    return;
  }

  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);
  switch (state.role) {
  case ui::AccessibilityTypes::ROLE_ALERT:
  case ui::AccessibilityTypes::ROLE_WINDOW:
    SendWindowNotification(view, type, profile);
    break;
  case ui::AccessibilityTypes::ROLE_BUTTONMENU:
  case ui::AccessibilityTypes::ROLE_MENUBAR:
  case ui::AccessibilityTypes::ROLE_MENUPOPUP:
    SendMenuNotification(view, type, profile);
    break;
  case ui::AccessibilityTypes::ROLE_BUTTONDROPDOWN:
  case ui::AccessibilityTypes::ROLE_PUSHBUTTON:
    SendButtonNotification(view, type, profile);
    break;
  case ui::AccessibilityTypes::ROLE_CHECKBUTTON:
    SendCheckboxNotification(view, type, profile);
    break;
  case ui::AccessibilityTypes::ROLE_COMBOBOX:
    SendComboboxNotification(view, type, profile);
    break;
  case ui::AccessibilityTypes::ROLE_LINK:
    SendLinkNotification(view, type, profile);
    break;
  case ui::AccessibilityTypes::ROLE_LOCATION_BAR:
  case ui::AccessibilityTypes::ROLE_TEXT:
    SendTextfieldNotification(view, type, profile);
    break;
  case ui::AccessibilityTypes::ROLE_MENUITEM:
    SendMenuItemNotification(view, type, profile);
    break;
  case ui::AccessibilityTypes::ROLE_RADIOBUTTON:
    // Not used anymore?
  case ui::AccessibilityTypes::ROLE_SLIDER:
    SendSliderNotification(view, type, profile);
    break;
  default:
    // If this is encountered, please file a bug with the role that wasn't
    // caught so we can add accessibility extension API support.
    NOTREACHED();
  }
}

// static
void AccessibilityEventRouterViews::SendButtonNotification(
    views::View* view,
    int type,
    Profile* profile) {
  AccessibilityButtonInfo info(
      profile, GetViewName(view), GetViewContext(view));
  SendAccessibilityNotification(type, &info);
}

// static
void AccessibilityEventRouterViews::SendLinkNotification(
    views::View* view,
    int type,
    Profile* profile) {
  AccessibilityLinkInfo info(profile, GetViewName(view), GetViewContext(view));
  SendAccessibilityNotification(type, &info);
}

// static
void AccessibilityEventRouterViews::SendMenuNotification(
    views::View* view,
    int type,
    Profile* profile) {
  AccessibilityMenuInfo info(profile, GetViewName(view));
  SendAccessibilityNotification(type, &info);
}

// static
void AccessibilityEventRouterViews::SendMenuItemNotification(
    views::View* view,
    int type,
    Profile* profile) {
  std::string name = GetViewName(view);
  std::string context = GetViewContext(view);

  bool has_submenu = false;
  int index = -1;
  int count = -1;

  if (view->GetClassName() == views::MenuItemView::kViewClassName)
    has_submenu = static_cast<views::MenuItemView*>(view)->HasSubmenu();

  views::View* parent_menu = view->parent();
  while (parent_menu != NULL && parent_menu->GetClassName() !=
         views::SubmenuView::kViewClassName) {
    parent_menu = parent_menu->parent();
  }
  if (parent_menu) {
    count = 0;
    RecursiveGetMenuItemIndexAndCount(parent_menu, view, &index, &count);
  }

  AccessibilityMenuItemInfo info(
      profile, name, context, has_submenu, index, count);
  SendAccessibilityNotification(type, &info);
}

// static
void AccessibilityEventRouterViews::SendTextfieldNotification(
    views::View* view,
    int type,
    Profile* profile) {
  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);
  std::string name = UTF16ToUTF8(state.name);
  std::string context = GetViewContext(view);
  bool password =
      (state.state & ui::AccessibilityTypes::STATE_PROTECTED) != 0;
  AccessibilityTextBoxInfo info(profile, name, context, password);
  std::string value = UTF16ToUTF8(state.value);
  info.SetValue(value, state.selection_start, state.selection_end);
  SendAccessibilityNotification(type, &info);
}

// static
void AccessibilityEventRouterViews::SendComboboxNotification(
    views::View* view,
    int type,
    Profile* profile) {
  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);
  std::string name = UTF16ToUTF8(state.name);
  std::string value = UTF16ToUTF8(state.value);
  std::string context = GetViewContext(view);
  AccessibilityComboBoxInfo info(
      profile, name, context, value, state.index, state.count);
  SendAccessibilityNotification(type, &info);
}

// static
void AccessibilityEventRouterViews::SendCheckboxNotification(
    views::View* view,
    int type,
    Profile* profile) {
  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);
  std::string name = UTF16ToUTF8(state.name);
  std::string value = UTF16ToUTF8(state.value);
  std::string context = GetViewContext(view);
  AccessibilityCheckboxInfo info(
      profile,
      name,
      context,
      state.state == ui::AccessibilityTypes::STATE_CHECKED);
  SendAccessibilityNotification(type, &info);
}

// static
void AccessibilityEventRouterViews::SendWindowNotification(
    views::View* view,
    int type,
    Profile* profile) {
  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);
  std::string window_text;

  // If it's an alert, try to get the text from the contents of the
  // static text, not the window title.
  if (state.role == ui::AccessibilityTypes::ROLE_ALERT)
    window_text = RecursiveGetStaticText(view);

  // Otherwise get it from the window's accessible name.
  if (window_text.empty())
    window_text = UTF16ToUTF8(state.name);

  AccessibilityWindowInfo info(profile, window_text);
  SendAccessibilityNotification(type, &info);
}

// static
void AccessibilityEventRouterViews::SendSliderNotification(
    views::View* view,
    int type,
    Profile* profile) {
  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);

  std::string name = UTF16ToUTF8(state.name);
  std::string value = UTF16ToUTF8(state.value);
  std::string context = GetViewContext(view);
  AccessibilitySliderInfo info(
      profile,
      name,
      context,
      value);
  SendAccessibilityNotification(type, &info);
}

// static
std::string AccessibilityEventRouterViews::GetViewName(views::View* view) {
  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);
  return UTF16ToUTF8(state.name);
}

// static
std::string AccessibilityEventRouterViews::GetViewContext(views::View* view) {
  for (views::View* parent = view->parent();
       parent;
       parent = parent->parent()) {
    ui::AccessibleViewState state;
    parent->GetAccessibleState(&state);

    // Two cases are handled right now. More could be added in the future
    // depending on how the UI evolves.

    // A control in a toolbar should use the toolbar's accessible name
    // as the context.
    if (state.role == ui::AccessibilityTypes::ROLE_TOOLBAR &&
        !state.name.empty()) {
      return UTF16ToUTF8(state.name);
    }

    // A control inside of an alert (like an infobar) should grab the
    // first static text descendant as the context; that's the prompt.
    if (state.role == ui::AccessibilityTypes::ROLE_ALERT) {
      views::View* static_text_child = FindDescendantWithAccessibleRole(
          parent, ui::AccessibilityTypes::ROLE_STATICTEXT);
      if (static_text_child) {
        ui::AccessibleViewState state;
        static_text_child->GetAccessibleState(&state);
        if (!state.name.empty())
          return UTF16ToUTF8(state.name);
      }
      return std::string();
    }
  }

  return std::string();
}

// static
views::View* AccessibilityEventRouterViews::FindDescendantWithAccessibleRole(
    views::View* view, ui::AccessibilityTypes::Role role) {
  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);
  if (state.role == role)
    return view;

  for (int i = 0; i < view->child_count(); i++) {
    views::View* child = view->child_at(i);
    views::View* result = FindDescendantWithAccessibleRole(child, role);
    if (result)
      return result;
  }

  return NULL;
}

// static
bool AccessibilityEventRouterViews::IsMenuEvent(
    views::View* view,
    int type) {
  if (type == chrome::NOTIFICATION_ACCESSIBILITY_MENU_OPENED ||
      type == chrome::NOTIFICATION_ACCESSIBILITY_MENU_CLOSED)
    return true;

  while (view) {
    ui::AccessibleViewState state;
    view->GetAccessibleState(&state);
    ui::AccessibilityTypes::Role role = state.role;
    if (role == ui::AccessibilityTypes::ROLE_MENUITEM ||
        role == ui::AccessibilityTypes::ROLE_MENUPOPUP) {
      return true;
    }
    view = view->parent();
  }

  return false;
}

// static
void AccessibilityEventRouterViews::RecursiveGetMenuItemIndexAndCount(
    views::View* menu,
    views::View* item,
    int* index,
    int* count) {
  for (int i = 0; i < menu->child_count(); ++i) {
    views::View* child = menu->child_at(i);
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

// static
std::string AccessibilityEventRouterViews::RecursiveGetStaticText(
    views::View* view) {
  ui::AccessibleViewState state;
  view->GetAccessibleState(&state);
  if (state.role == ui::AccessibilityTypes::ROLE_STATICTEXT)
    return UTF16ToUTF8(state.name);

  for (int i = 0; i < view->child_count(); ++i) {
    views::View* child = view->child_at(i);
    std::string result = RecursiveGetStaticText(child);
    if (!result.empty())
      return result;
  }
  return std::string();
}
