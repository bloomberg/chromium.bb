// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/accessibility_event_router_views.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/notification_service.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using views::FocusManager;
using views::ViewStorage;

AccessibilityEventRouterViews::AccessibilityEventRouterViews()
    : most_recent_profile_(NULL),
      most_recent_view_id_(
          ViewStorage::GetInstance()->CreateStorageID()) {
  // Register for notification when profile is destroyed to ensure that all
  // observers are detatched at that time.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
}

AccessibilityEventRouterViews::~AccessibilityEventRouterViews() {
}

// static
AccessibilityEventRouterViews* AccessibilityEventRouterViews::GetInstance() {
  return Singleton<AccessibilityEventRouterViews>::get();
}

void AccessibilityEventRouterViews::HandleAccessibilityEvent(
    views::View* view, ui::AXEvent event_type) {
  if (!ExtensionAccessibilityEventRouter::GetInstance()->
      IsAccessibilityEnabled()) {
    return;
  }

  if (event_type == ui::AX_EVENT_TEXT_CHANGED ||
      event_type == ui::AX_EVENT_TEXT_SELECTION_CHANGED) {
    // These two events should only be sent for views that have focus. This
    // enforces the invariant that we fire events triggered by user action and
    // not by programmatic logic. For example, the location bar can be updated
    // by javascript while the user focus is within some other part of the
    // user interface. In contrast, the other supported events here do not
    // depend on focus. For example, a menu within a menubar can open or close
    // while focus is within the location bar or anywhere else as a result of
    // user action. Note that the below logic can at some point be removed if
    // we pass more information along to the listener such as focused state.
    if (!view->GetFocusManager() ||
        view->GetFocusManager()->GetFocusedView() != view)
      return;
  }

  // Don't dispatch the accessibility event until the next time through the
  // event loop, to handle cases where the view's state changes after
  // the call to post the event. It's safe to use base::Unretained(this)
  // because AccessibilityEventRouterViews is a singleton.
  ViewStorage* view_storage = ViewStorage::GetInstance();
  int view_storage_id = view_storage->CreateStorageID();
  view_storage->StoreView(view_storage_id, view);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &AccessibilityEventRouterViews::DispatchEventOnViewStorageId,
          view_storage_id,
          event_type));
}

void AccessibilityEventRouterViews::HandleMenuItemFocused(
    const base::string16& menu_name,
    const base::string16& menu_item_name,
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
                                 base::UTF16ToUTF8(menu_item_name),
                                 base::UTF16ToUTF8(menu_name),
                                 has_submenu,
                                 item_index,
                                 item_count);
  SendControlAccessibilityNotification(
      ui::AX_EVENT_FOCUS, &info);
}

void AccessibilityEventRouterViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_PROFILE_DESTROYED);
  Profile* profile = content::Source<Profile>(source).ptr();
  if (profile == most_recent_profile_)
    most_recent_profile_ = NULL;
}

//
// Private methods
//

void AccessibilityEventRouterViews::DispatchEventOnViewStorageId(
    int view_storage_id,
    ui::AXEvent type) {
  ViewStorage* view_storage = ViewStorage::GetInstance();
  views::View* view = view_storage->RetrieveView(view_storage_id);
  view_storage->RemoveView(view_storage_id);
  if (!view)
    return;

  AccessibilityEventRouterViews* instance =
      AccessibilityEventRouterViews::GetInstance();
  instance->DispatchAccessibilityEvent(view, type);
}

void AccessibilityEventRouterViews::DispatchAccessibilityEvent(
    views::View* view, ui::AXEvent type) {
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
  if (!profile) {
    if (g_browser_process->profile_manager())
      profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  }
  if (!profile) {
    LOG(WARNING) << "Accessibility notification but no profile";
    return;
  }

  most_recent_profile_ = profile;

  if (type == ui::AX_EVENT_MENU_START ||
      type == ui::AX_EVENT_MENU_POPUP_START ||
      type == ui::AX_EVENT_MENU_END ||
      type == ui::AX_EVENT_MENU_POPUP_END) {
    SendMenuNotification(view, type, profile);
    return;
  }

  view = FindFirstAccessibleAncestor(view);

  // Since multiple items could share a highest focusable view, these items
  // could all dispatch the same accessibility hover events, which isn't
  // necessary.
  if (type == ui::AX_EVENT_HOVER &&
      ViewStorage::GetInstance()->RetrieveView(most_recent_view_id_) == view) {
    return;
  }
  // If there was already a view stored here from before, it must be removed
  // before storing a new view.
  ViewStorage::GetInstance()->RemoveView(most_recent_view_id_);
  ViewStorage::GetInstance()->StoreView(most_recent_view_id_, view);

  ui::AXViewState state;
  view->GetAccessibleState(&state);

  if (type == ui::AX_EVENT_ALERT &&
      !(state.role == ui::AX_ROLE_ALERT ||
        state.role == ui::AX_ROLE_WINDOW)) {
    SendAlertControlNotification(view, type, profile);
    return;
  }

  switch (state.role) {
  case ui::AX_ROLE_ALERT:
  case ui::AX_ROLE_DIALOG:
  case ui::AX_ROLE_WINDOW:
    SendWindowNotification(view, type, profile);
    break;
  case ui::AX_ROLE_POP_UP_BUTTON:
  case ui::AX_ROLE_MENU_BAR:
  case ui::AX_ROLE_MENU_LIST_POPUP:
    SendMenuNotification(view, type, profile);
    break;
  case ui::AX_ROLE_BUTTON_DROP_DOWN:
  case ui::AX_ROLE_BUTTON:
    SendButtonNotification(view, type, profile);
    break;
  case ui::AX_ROLE_CHECK_BOX:
    SendCheckboxNotification(view, type, profile);
    break;
  case ui::AX_ROLE_COMBO_BOX:
    SendComboboxNotification(view, type, profile);
    break;
  case ui::AX_ROLE_LINK:
    SendLinkNotification(view, type, profile);
    break;
  case ui::AX_ROLE_LOCATION_BAR:
  case ui::AX_ROLE_TEXT_FIELD:
    SendTextfieldNotification(view, type, profile);
    break;
  case ui::AX_ROLE_MENU_ITEM:
    SendMenuItemNotification(view, type, profile);
    break;
  case ui::AX_ROLE_RADIO_BUTTON:
    // Not used anymore?
  case ui::AX_ROLE_SLIDER:
    SendSliderNotification(view, type, profile);
    break;
  case ui::AX_ROLE_STATIC_TEXT:
    SendStaticTextNotification(view, type, profile);
    break;
  case ui::AX_ROLE_TREE:
    SendTreeNotification(view, type, profile);
    break;
  case ui::AX_ROLE_TAB:
    SendTabNotification(view, type, profile);
    break;
  case ui::AX_ROLE_TREE_ITEM:
    SendTreeItemNotification(view, type, profile);
    break;
  default:
    // Hover events can fire on literally any view, so it's safe to
    // ignore ones we don't care about.
    if (type == ui::AX_EVENT_HOVER)
      break;

    // If this is encountered, please file a bug with the role that wasn't
    // caught so we can add accessibility extension API support.
    NOTREACHED();
  }
}

// static
void AccessibilityEventRouterViews::SendTabNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);
  if (state.index == -1)
    return;
  std::string name = base::UTF16ToUTF8(state.name);
  std::string context = GetViewContext(view);
  AccessibilityTabInfo info(profile, name, context, state.index, state.count);
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendButtonNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  AccessibilityButtonInfo info(
      profile, GetViewName(view), GetViewContext(view));
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendStaticTextNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  AccessibilityStaticTextInfo info(
      profile, GetViewName(view), GetViewContext(view));
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendLinkNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  AccessibilityLinkInfo info(profile, GetViewName(view), GetViewContext(view));
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendMenuNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  AccessibilityMenuInfo info(profile, GetViewName(view));
  SendMenuAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendMenuItemNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  std::string name = GetViewName(view);
  std::string context = GetViewContext(view);

  bool has_submenu = false;
  int index = -1;
  int count = -1;

  if (!strcmp(view->GetClassName(), views::MenuItemView::kViewClassName))
    has_submenu = static_cast<views::MenuItemView*>(view)->HasSubmenu();

  views::View* parent_menu = view->parent();
  while (parent_menu != NULL && strcmp(parent_menu->GetClassName(),
                                       views::SubmenuView::kViewClassName)) {
    parent_menu = parent_menu->parent();
  }
  if (parent_menu) {
    count = 0;
    RecursiveGetMenuItemIndexAndCount(parent_menu, view, &index, &count);
  }

  AccessibilityMenuItemInfo info(
      profile, name, context, has_submenu, index, count);
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendTreeNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  AccessibilityTreeInfo info(profile, GetViewName(view));
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendTreeItemNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  std::string name = GetViewName(view);
  std::string context = GetViewContext(view);

  if (strcmp(view->GetClassName(), views::TreeView::kViewClassName) != 0) {
    NOTREACHED();
    return;
  }

  views::TreeView* tree = static_cast<views::TreeView*>(view);
  ui::TreeModelNode* selected_node = tree->GetSelectedNode();
  ui::TreeModel* model = tree->model();

  int siblings_count = model->GetChildCount(model->GetRoot());
  int children_count = -1;
  int index = -1;
  int depth = -1;
  bool is_expanded = false;

  if (selected_node) {
    children_count = model->GetChildCount(selected_node);
    is_expanded = tree->IsExpanded(selected_node);
    ui::TreeModelNode* parent_node = model->GetParent(selected_node);
    if (parent_node) {
      index = model->GetIndexOf(parent_node, selected_node);
      siblings_count = model->GetChildCount(parent_node);
    }
    // Get node depth.
    depth = 0;
    while (parent_node) {
      depth++;
      parent_node = model->GetParent(parent_node);
    }
  }

  AccessibilityTreeItemInfo info(
      profile, name, context, depth, index, siblings_count, children_count,
      is_expanded);
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendTextfieldNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);
  std::string name = base::UTF16ToUTF8(state.name);
  std::string context = GetViewContext(view);
  bool password = state.HasStateFlag(ui::AX_STATE_PROTECTED);
  AccessibilityTextBoxInfo info(profile, name, context, password);
  std::string value = base::UTF16ToUTF8(state.value);
  info.SetValue(value, state.selection_start, state.selection_end);
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendComboboxNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);
  std::string name = base::UTF16ToUTF8(state.name);
  std::string value = base::UTF16ToUTF8(state.value);
  std::string context = GetViewContext(view);
  AccessibilityComboBoxInfo info(
      profile, name, context, value, state.index, state.count);
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendCheckboxNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);
  std::string name = base::UTF16ToUTF8(state.name);
  std::string context = GetViewContext(view);
  AccessibilityCheckboxInfo info(
      profile,
      name,
      context,
      state.HasStateFlag(ui::AX_STATE_CHECKED));
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendWindowNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);
  std::string window_text;

  // If it's an alert, try to get the text from the contents of the
  // static text, not the window title.
  if (state.role == ui::AX_ROLE_ALERT)
    window_text = RecursiveGetStaticText(view);

  // Otherwise get it from the window's accessible name.
  if (window_text.empty())
    window_text = base::UTF16ToUTF8(state.name);

  AccessibilityWindowInfo info(profile, window_text);
  SendWindowAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendSliderNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);

  std::string name = base::UTF16ToUTF8(state.name);
  std::string value = base::UTF16ToUTF8(state.value);
  std::string context = GetViewContext(view);
  AccessibilitySliderInfo info(
      profile,
      name,
      context,
      value);
  SendControlAccessibilityNotification(event, &info);
}

// static
void AccessibilityEventRouterViews::SendAlertControlNotification(
    views::View* view,
    ui::AXEvent event,
    Profile* profile) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);

  std::string name = base::UTF16ToUTF8(state.name);
  AccessibilityAlertInfo info(
      profile,
      name);
  SendControlAccessibilityNotification(event, &info);
}

// static
std::string AccessibilityEventRouterViews::GetViewName(views::View* view) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);
  return base::UTF16ToUTF8(state.name);
}

// static
std::string AccessibilityEventRouterViews::GetViewContext(views::View* view) {
  for (views::View* parent = view->parent();
       parent;
       parent = parent->parent()) {
    ui::AXViewState state;
    parent->GetAccessibleState(&state);

    // Two cases are handled right now. More could be added in the future
    // depending on how the UI evolves.

    // A control inside of alert, toolbar or dialog should use that container's
    // accessible name.
    if ((state.role == ui::AX_ROLE_ALERT ||
         state.role == ui::AX_ROLE_DIALOG ||
         state.role == ui::AX_ROLE_TOOLBAR) &&
        !state.name.empty()) {
      return base::UTF16ToUTF8(state.name);
    }

    // A control inside of an alert or dialog (including an infobar)
    // should grab the first static text descendant as the context;
    // that's the prompt.
    if (state.role == ui::AX_ROLE_ALERT ||
        state.role == ui::AX_ROLE_DIALOG) {
      views::View* static_text_child = FindDescendantWithAccessibleRole(
          parent, ui::AX_ROLE_STATIC_TEXT);
      if (static_text_child) {
        ui::AXViewState state;
        static_text_child->GetAccessibleState(&state);
        if (!state.name.empty())
          return base::UTF16ToUTF8(state.name);
      }
      return std::string();
    }
  }

  return std::string();
}

// static
views::View* AccessibilityEventRouterViews::FindDescendantWithAccessibleRole(
    views::View* view, ui::AXRole role) {
  ui::AXViewState state;
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
void AccessibilityEventRouterViews::RecursiveGetMenuItemIndexAndCount(
    views::View* menu,
    views::View* item,
    int* index,
    int* count) {
  for (int i = 0; i < menu->child_count(); ++i) {
    views::View* child = menu->child_at(i);
    if (!child->visible())
      continue;

    int previous_count = *count;
    RecursiveGetMenuItemIndexAndCount(child, item, index, count);
    ui::AXViewState state;
    child->GetAccessibleState(&state);
    if (state.role == ui::AX_ROLE_MENU_ITEM &&
        *count == previous_count) {
      if (item == child)
        *index = *count;
      (*count)++;
    } else if (state.role == ui::AX_ROLE_BUTTON) {
      if (item == child)
        *index = *count;
      (*count)++;
    }
  }
}

// static
std::string AccessibilityEventRouterViews::RecursiveGetStaticText(
    views::View* view) {
  ui::AXViewState state;
  view->GetAccessibleState(&state);
  if (state.role == ui::AX_ROLE_STATIC_TEXT)
    return base::UTF16ToUTF8(state.name);

  for (int i = 0; i < view->child_count(); ++i) {
    views::View* child = view->child_at(i);
    std::string result = RecursiveGetStaticText(child);
    if (!result.empty())
      return result;
  }
  return std::string();
}

// static
views::View* AccessibilityEventRouterViews::FindFirstAccessibleAncestor(
    views::View* view) {
  views::View* temp_view = view;
  while (temp_view->parent() && !temp_view->IsAccessibilityFocusable())
    temp_view = temp_view->parent();
  if (temp_view->IsAccessibilityFocusable())
    return temp_view;
  return view;
}
