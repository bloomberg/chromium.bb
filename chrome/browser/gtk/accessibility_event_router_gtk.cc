// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/accessibility_event_router_gtk.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_type.h"
#include "views/controls/textfield/native_textfield_gtk.h"

#if defined(TOOLKIT_VIEWS)
#include "views/controls/textfield/gtk_views_textview.h"
#include "views/controls/textfield/gtk_views_entry.h"
#endif

namespace {

//
// Callbacks triggered by signals on gtk widgets.
//

gboolean OnWidgetFocused(GSignalInvocationHint *ihint,
                         guint n_param_values,
                         const GValue* param_values,
                         gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_FOCUSED);
  return TRUE;
}

gboolean OnButtonClicked(GSignalInvocationHint *ihint,
                         guint n_param_values,
                         const GValue* param_values,
                         gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  // Skip toggle buttons because we're also listening on "toggle" events.
  if (GTK_IS_TOGGLE_BUTTON(widget))
    return true;
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
  return TRUE;
}

gboolean OnButtonToggled(GSignalInvocationHint *ihint,
                         guint n_param_values,
                         const GValue* param_values,
                         gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  bool checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  // Skip propagating an "uncheck" event for a radio button because it's
  // redundant; there will always be a corresponding "check" event for
  // a different radio button the group.
  if (GTK_IS_RADIO_BUTTON(widget) && !checked)
    return true;
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
  return TRUE;
}

gboolean OnPageSwitched(GSignalInvocationHint *ihint,
                        guint n_param_values,
                        const GValue* param_values,
                        gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  // The page hasn't switched yet, so defer calling
  // DispatchAccessibilityNotification.
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      PostDispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
  return TRUE;
}

gboolean OnComboBoxChanged(GSignalInvocationHint *ihint,
                           guint n_param_values,
                           const GValue* param_values,
                           gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  if (!GTK_IS_COMBO_BOX(widget))
    return true;
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
  return TRUE;
}

gboolean OnTreeViewCursorChanged(GSignalInvocationHint *ihint,
                                 guint n_param_values,
                                 const GValue* param_values,
                                 gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  if (!GTK_IS_TREE_VIEW(widget)) {
    return true;
  }
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
  return TRUE;
}

gboolean OnEntryChanged(GSignalInvocationHint *ihint,
                        guint n_param_values,
                        const GValue* param_values,
                        gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  if (!GTK_IS_ENTRY(widget)) {
    return TRUE;
  }
  // The text hasn't changed yet, so defer calling
  // DispatchAccessibilityNotification.
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      PostDispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_TEXT_CHANGED);
  return TRUE;
}

gboolean OnTextBufferChanged(GSignalInvocationHint *ihint,
                             guint n_param_values,
                             const GValue* param_values,
                             gpointer user_data) {
  // The text hasn't changed yet, so defer calling
  // DispatchAccessibilityNotification.
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      PostDispatchAccessibilityNotification(
          NULL, NotificationType::ACCESSIBILITY_TEXT_CHANGED);
  return TRUE;
}

gboolean OnTextViewChanged(GSignalInvocationHint *ihint,
                           guint n_param_values,
                           const GValue* param_values,
                           gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  if (!GTK_IS_TEXT_VIEW(widget)) {
    return TRUE;
  }
  // The text hasn't changed yet, so defer calling
  // DispatchAccessibilityNotification.
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      PostDispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_TEXT_CHANGED);
  return TRUE;
}

gboolean OnMenuMoveCurrent(GSignalInvocationHint *ihint,
                           guint n_param_values,
                           const GValue* param_values,
                           gpointer user_data) {
  // Get the widget (the GtkMenu).
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));

  // Moving may move us into or out of a submenu, so after the menu
  // item moves, |widget| may not be valid anymore. To be safe, then,
  // find the topmost ancestor of this menu and post the notification
  // dispatch on that menu. Then the dispatcher will recurse into submenus
  // as necessary to figure out which item is focused.
  while (GTK_MENU_SHELL(widget)->parent_menu_shell)
    widget = GTK_MENU_SHELL(widget)->parent_menu_shell;

  // The menu item hasn't moved yet, so we want to defer calling
  // DispatchAccessibilityNotification until after it does.
  reinterpret_cast<AccessibilityEventRouterGtk*>(user_data)->
      PostDispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_FOCUSED);
  return TRUE;
}

}  // anonymous namespace

AccessibilityEventRouterGtk::AccessibilityEventRouterGtk()
    : listening_(false),
      most_recent_profile_(NULL),
      most_recent_widget_(NULL),
      method_factory_(this) {
  // We don't want our event listeners to be installed if accessibility is
  // disabled. Install listeners so we can install and uninstall them as
  // needed, then install them now if it's currently enabled.
  ExtensionAccessibilityEventRouter *extension_event_router =
      ExtensionAccessibilityEventRouter::GetInstance();
  extension_event_router->AddOnEnabledListener(
      NewCallback(this,
                  &AccessibilityEventRouterGtk::InstallEventListeners));
  extension_event_router->AddOnDisabledListener(
      NewCallback(this,
                  &AccessibilityEventRouterGtk::RemoveEventListeners));
  if (extension_event_router->IsAccessibilityEnabled()) {
    InstallEventListeners();
  }
}

AccessibilityEventRouterGtk::~AccessibilityEventRouterGtk() {
  RemoveEventListeners();
}

// static
AccessibilityEventRouterGtk* AccessibilityEventRouterGtk::GetInstance() {
  return Singleton<AccessibilityEventRouterGtk>::get();
}

void AccessibilityEventRouterGtk::InstallEventListener(
    const char* signal_name,
    GType widget_type,
    GSignalEmissionHook hook_func) {
  guint signal_id = g_signal_lookup(signal_name, widget_type);
  gulong hook_id = g_signal_add_emission_hook(
      signal_id, 0, hook_func, reinterpret_cast<gpointer>(this), NULL);
  installed_hooks_.push_back(InstalledHook(signal_id, hook_id));
}

bool AccessibilityEventRouterGtk::IsPassword(GtkWidget* widget) {
  bool is_password = false;
#if defined (TOOLKIT_VIEWS)
  is_password = (GTK_IS_ENTRY(widget) &&
                 GTK_VIEWS_ENTRY(widget)->host != NULL &&
                 GTK_VIEWS_ENTRY(widget)->host->IsPassword()) ||
                (GTK_IS_TEXT_VIEW(widget) &&
                 GTK_VIEWS_TEXTVIEW(widget)->host != NULL &&
                 GTK_VIEWS_TEXTVIEW(widget)->host->IsPassword());
#endif
  return is_password;
}


void AccessibilityEventRouterGtk::InstallEventListeners() {
  // Create and destroy each type of widget we need signals for,
  // to ensure their modules are loaded, otherwise g_signal_lookup
  // might fail.
  g_object_unref(g_object_ref_sink(gtk_combo_box_new()));
  g_object_unref(g_object_ref_sink(gtk_entry_new()));
  g_object_unref(g_object_ref_sink(gtk_notebook_new()));
  g_object_unref(g_object_ref_sink(gtk_toggle_button_new()));
  g_object_unref(g_object_ref_sink(gtk_tree_view_new()));
  g_object_unref(g_object_ref_sink(gtk_text_view_new()));
  g_object_unref(g_object_ref_sink(gtk_text_buffer_new(NULL)));

  // Add signal emission hooks for the events we're interested in.
  InstallEventListener("clicked", GTK_TYPE_BUTTON, OnButtonClicked);
  InstallEventListener("changed", GTK_TYPE_COMBO_BOX, OnComboBoxChanged);
  InstallEventListener("cursor-changed", GTK_TYPE_TREE_VIEW,
                       OnTreeViewCursorChanged);
  InstallEventListener("changed", GTK_TYPE_ENTRY, OnEntryChanged);
  InstallEventListener("insert-text", GTK_TYPE_ENTRY, OnEntryChanged);
  InstallEventListener("delete-text", GTK_TYPE_ENTRY, OnEntryChanged);
  InstallEventListener("move-cursor", GTK_TYPE_ENTRY, OnEntryChanged);
  InstallEventListener("focus-in-event", GTK_TYPE_WIDGET, OnWidgetFocused);
  InstallEventListener("switch-page", GTK_TYPE_NOTEBOOK, OnPageSwitched);
  InstallEventListener("toggled", GTK_TYPE_TOGGLE_BUTTON, OnButtonToggled);
  InstallEventListener("move-current", GTK_TYPE_MENU, OnMenuMoveCurrent);
  InstallEventListener("changed", GTK_TYPE_TEXT_BUFFER, OnTextBufferChanged);
  InstallEventListener("move-cursor", GTK_TYPE_TEXT_VIEW, OnTextViewChanged);

  listening_ = true;
}

void AccessibilityEventRouterGtk::RemoveEventListeners() {
  for (size_t i = 0; i < installed_hooks_.size(); i++) {
    g_signal_remove_emission_hook(
        installed_hooks_[i].signal_id,
        installed_hooks_[i].hook_id);
  }
  installed_hooks_.clear();

  listening_ = false;
}

void AccessibilityEventRouterGtk::AddRootWidget(
    GtkWidget* root_widget, Profile* profile) {
  root_widget_profile_map_[root_widget] = profile;
}

void AccessibilityEventRouterGtk::RemoveRootWidget(GtkWidget* root_widget) {
  DCHECK(root_widget_profile_map_.find(root_widget) !=
         root_widget_profile_map_.end());
  root_widget_profile_map_.erase(root_widget);
}

void AccessibilityEventRouterGtk::IgnoreWidget(GtkWidget* widget) {
  widget_info_map_[widget].ignore = true;
}

void AccessibilityEventRouterGtk::SetWidgetName(
    GtkWidget* widget, std::string name) {
  widget_info_map_[widget].name = name;
}

void AccessibilityEventRouterGtk::RemoveWidget(GtkWidget* widget) {
  DCHECK(widget_info_map_.find(widget) != widget_info_map_.end());
  widget_info_map_.erase(widget);
}

void AccessibilityEventRouterGtk::FindWidget(
    GtkWidget* widget, Profile** profile, bool* is_accessible) {
  *is_accessible = false;

  // First see if it's a descendant of a root widget.
  for (base::hash_map<GtkWidget*, Profile*>::const_iterator iter =
           root_widget_profile_map_.begin();
       iter != root_widget_profile_map_.end();
       ++iter) {
    if (gtk_widget_is_ancestor(widget, iter->first)) {
      *is_accessible = true;
      if (profile)
        *profile = iter->second;
      break;
    }
  }
  if (!*is_accessible)
    return;

  // Now make sure it's not marked as a widget to be ignored.
  base::hash_map<GtkWidget*, WidgetInfo>::const_iterator iter =
      widget_info_map_.find(widget);
  if (iter != widget_info_map_.end() && iter->second.ignore) {
    *is_accessible = false;
    return;
  }
}

std::string AccessibilityEventRouterGtk::GetWidgetName(GtkWidget* widget) {
  base::hash_map<GtkWidget*, WidgetInfo>::const_iterator iter =
      widget_info_map_.find(widget);
  if (iter != widget_info_map_.end()) {
    return iter->second.name;
  } else {
    return "";
  }
}

void AccessibilityEventRouterGtk::StartListening() {
  listening_ = true;
}

void AccessibilityEventRouterGtk::StopListening() {
  listening_ = false;
}

void AccessibilityEventRouterGtk::DispatchAccessibilityNotification(
    GtkWidget* widget, NotificationType type) {
  // If there's no message loop, we must be about to shutdown or we're
  // running inside a test; either way, there's no reason to do any
  // further processing.
  if (!MessageLoop::current())
    return;

  if (!listening_)
    return;

  Profile* profile = NULL;
  bool is_accessible;

  // Special case: when we get ACCESSIBILITY_TEXT_CHANGED, we don't get
  // a pointer to the widget, so we try to retrieve it from the most recent
  // widget.
  if (widget == NULL &&
      type == NotificationType::ACCESSIBILITY_TEXT_CHANGED &&
      most_recent_widget_ &&
      GTK_IS_TEXT_VIEW(most_recent_widget_)) {
    widget = most_recent_widget_;
  }

  if (!widget)
    return;

  most_recent_widget_ = widget;
  FindWidget(widget, &profile, &is_accessible);
  if (profile)
    most_recent_profile_ = profile;

  // Special case: a GtkMenu isn't associated with any particular
  // toplevel window, so menu events get routed to the profile of
  // the most recent event that was associated with a window.
  if (GTK_IS_MENU_SHELL(widget) && most_recent_profile_) {
    SendMenuItemNotification(widget, type, most_recent_profile_);
    return;
  }

  // In all other cases, return if this widget wasn't marked as accessible.
  if (!is_accessible)
    return;

  // The order of these checks matters, because, for example, a radio button
  // is a subclass of button, and a combo box is a composite control where
  // the focus event goes to the button that's a child of the combo box.
  GtkWidget* parent = gtk_widget_get_parent(widget);
  if (parent && GTK_IS_BUTTON(widget) && GTK_IS_TREE_VIEW(parent)) {
    // This is a list box column header.  Currently not supported.
    return;
  } else if (GTK_IS_COMBO_BOX(widget)) {
    SendComboBoxNotification(widget, type, profile);
  } else if (parent && GTK_IS_COMBO_BOX(parent)) {
    SendComboBoxNotification(parent, type, profile);
  } else if (GTK_IS_RADIO_BUTTON(widget)) {
    SendRadioButtonNotification(widget, type, profile);
  } else if (GTK_IS_TOGGLE_BUTTON(widget)) {
    SendCheckboxNotification(widget, type, profile);
  } else if (GTK_IS_BUTTON(widget)) {
    SendButtonNotification(widget, type, profile);
  } else if (GTK_IS_ENTRY(widget)) {
    SendEntryNotification(widget, type, profile);
  } else if (GTK_IS_TEXT_VIEW(widget)) {
    SendTextViewNotification(widget, type, profile);
  } else if (GTK_IS_NOTEBOOK(widget)) {
    SendTabNotification(widget, type, profile);
  } else if (GTK_IS_TREE_VIEW(widget)) {
    SendListBoxNotification(widget, type, profile);
  } else {
    // If we have no idea what this control is, return and skip the
    // temporary pause in event listening.
    return;
  }

  // After this method returns, additional signal handlers will run,
  // which will sometimes generate additional signals.  To avoid
  // generating redundant accessibility notifications for the same
  // initial event, stop listening to all signals generated from now
  // until this posted task runs.
  StopListening();
  MessageLoop::current()->PostTask(
      FROM_HERE, method_factory_.NewRunnableMethod(
          &AccessibilityEventRouterGtk::StartListening));
}

void AccessibilityEventRouterGtk::PostDispatchAccessibilityNotification(
    GtkWidget* widget, NotificationType type) {
  if (!MessageLoop::current())
    return;

  MessageLoop::current()->PostTask(
      FROM_HERE, method_factory_.NewRunnableMethod(
          &AccessibilityEventRouterGtk::DispatchAccessibilityNotification,
          widget,
          type));
}

void AccessibilityEventRouterGtk::SendRadioButtonNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  // Get the radio button name
  std::string button_name = GetWidgetName(widget);
  if (button_name.empty() && gtk_button_get_label(GTK_BUTTON(widget)))
    button_name = gtk_button_get_label(GTK_BUTTON(widget));

  // Get its state
  bool checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

  // Get the index of this radio button and the total number of
  // radio buttons in the group.
  int item_count = 0;
  int item_index = -1;
  for (GSList* group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget));
       group;
       group = group->next) {
    if (group->data == widget) {
      item_index = item_count;
    }
    item_count++;
  }
  item_index = item_count - 1 - item_index;

  AccessibilityRadioButtonInfo info(
      profile, button_name, checked, item_index, item_count);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterGtk::SendCheckboxNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  std::string button_name = GetWidgetName(widget);
  if (button_name.empty() && gtk_button_get_label(GTK_BUTTON(widget)))
    button_name = gtk_button_get_label(GTK_BUTTON(widget));
  bool checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  AccessibilityCheckboxInfo info(profile, button_name, checked);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterGtk::SendButtonNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  std::string button_name = GetWidgetName(widget);
  if (button_name.empty() && gtk_button_get_label(GTK_BUTTON(widget)))
    button_name = gtk_button_get_label(GTK_BUTTON(widget));
  AccessibilityButtonInfo info(profile, button_name);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterGtk::SendEntryNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  std::string name = GetWidgetName(widget);
  std::string value = gtk_entry_get_text(GTK_ENTRY(widget));
  gint start_pos;
  gint end_pos;
  gtk_editable_get_selection_bounds(GTK_EDITABLE(widget), &start_pos, &end_pos);
  AccessibilityTextBoxInfo info(profile, name, IsPassword(widget));
  info.SetValue(value, start_pos, end_pos);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterGtk::SendTextViewNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  std::string name = GetWidgetName(widget);
  GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, false);
  std::string value = text;
  g_free(text);
  GtkTextIter sel_start, sel_end;
  gtk_text_buffer_get_selection_bounds(buffer, &sel_start, &sel_end);
  int start_pos = gtk_text_iter_get_offset(&sel_start);
  int end_pos = gtk_text_iter_get_offset(&sel_end);
  AccessibilityTextBoxInfo info(profile, name, IsPassword(widget));
  info.SetValue(value, start_pos, end_pos);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterGtk::SendTabNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  int index = gtk_notebook_get_current_page(GTK_NOTEBOOK(widget));
  int page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(widget));
  GtkWidget* page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(widget), index);
  GtkWidget* label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(widget), page);
  std::string name = GetWidgetName(widget);
  if (name.empty() && gtk_label_get_text(GTK_LABEL(label))) {
    name = gtk_label_get_text(GTK_LABEL(label));
  }
  AccessibilityTabInfo info(profile, name, index, page_count);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterGtk::SendComboBoxNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  // Get the index of the selected item.  Will return -1 if no item is
  // active, which matches the semantics of the extension API.
  int index = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

  // Get the number of items.
  GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
  int count = gtk_tree_model_iter_n_children(model, NULL);

  // Get the value of the current item, if possible.  Note that the
  // model behind the combo box could be arbitrarily complex in theory,
  // but this code just handles flat lists where the first string column
  // contains the display value.
  std::string value;
  int string_column_index = -1;
  for (int i = 0; i < gtk_tree_model_get_n_columns(model); i++) {
    if (gtk_tree_model_get_column_type(model, i) == G_TYPE_STRING) {
      string_column_index = i;
      break;
    }
  }
  if (string_column_index) {
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter)) {
      GValue gvalue = { 0 };
      gtk_tree_model_get_value(model, &iter, string_column_index, &gvalue);
      const char* string_value = g_value_get_string(&gvalue);
      if (string_value) {
        value = string_value;
      }
      g_value_unset(&gvalue);
    }
  } else {
    // Otherwise this must be a gtk_combo_box_text, in which case this
    // function will return the value of the current item, instead.
    value = gtk_combo_box_get_active_text(GTK_COMBO_BOX(widget));
  }

  // Get the name of this combo box.
  std::string name = GetWidgetName(widget);

  // Send the notification.
  AccessibilityComboBoxInfo info(profile, name, value, index, count);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterGtk::SendListBoxNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  // Get the number of items.
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
  int count = gtk_tree_model_iter_n_children(model, NULL);

  // Get the current selected index and its value.
  int index = -1;
  std::string value;
  GtkTreePath* path;
  gtk_tree_view_get_cursor(GTK_TREE_VIEW(widget), &path, NULL);
  if (path != NULL) {
    gint* indices = gtk_tree_path_get_indices(path);
    if (indices)
      index = indices[0];

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
      for (int i = 0; i < gtk_tree_model_get_n_columns(model); i++) {
        if (gtk_tree_model_get_column_type(model, i) == G_TYPE_STRING) {
          GValue gvalue = { 0 };
          gtk_tree_model_get_value(model, &iter, i, &gvalue);
          const char* string_value = g_value_get_string(&gvalue);
          if (string_value) {
            if (!value.empty())
              value += " ";
            value += string_value;
          }
          g_value_unset(&gvalue);
        }
      }
    }

    gtk_tree_path_free(path);
  }

  // Get the name of this control.
  std::string name = GetWidgetName(widget);

  // Send the notification.
  AccessibilityListBoxInfo info(profile, name, value, index, count);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouterGtk::SendMenuItemNotification(
    GtkWidget* menu, NotificationType type, Profile* profile) {
  // Find the focused menu item, recursing into submenus as needed.
  GtkWidget* menu_item = GTK_MENU_SHELL(menu)->active_menu_item;
  if (!menu_item)
    return;
  GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item));
  while (submenu && GTK_MENU_SHELL(submenu)->active_menu_item) {
    menu = submenu;
    menu_item = GTK_MENU_SHELL(menu)->active_menu_item;
    if (!menu_item)
      return;
    submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item));
  }

  // Figure out the item index and total number of items.
  GList* items = gtk_container_get_children(GTK_CONTAINER(menu));
  guint count = g_list_length(items);
  int index = g_list_index(items, static_cast<gconstpointer>(menu_item));

  // Get the menu item's label.
  std::string name;
#if GTK_CHECK_VERSION(2, 16, 0)
  name = gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item));
#else
  GList* children = gtk_container_get_children(GTK_CONTAINER(menu_item));
  for (GList* l = g_list_first(children); l != NULL; l = g_list_next(l)) {
    GtkWidget* child = static_cast<GtkWidget*>(l->data);
    if (GTK_IS_LABEL(child)) {
      name = gtk_label_get_label(GTK_LABEL(child));
      break;
    }
  }
#endif

  // Send the event.
  AccessibilityMenuItemInfo info(profile, name, submenu != NULL, index, count);
  SendAccessibilityNotification(type, &info);
}
