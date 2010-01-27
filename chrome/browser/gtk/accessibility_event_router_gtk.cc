// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/accessibility_event_router_gtk.h"

#include "base/basictypes.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_type.h"

namespace {

//
// Callbacks triggered by signals on gtk widgets.
//

gboolean OnWidgetFocused(GSignalInvocationHint *ihint,
                         guint n_param_values,
                         const GValue* param_values,
                         gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  reinterpret_cast<AccessibilityEventRouter *>(user_data)
      ->DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_FOCUSED);
  return true;
}

gboolean OnButtonClicked(GSignalInvocationHint *ihint,
                         guint n_param_values,
                         const GValue* param_values,
                         gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  // Skip toggle buttons because we're also listening on "toggle" events.
  if (GTK_IS_TOGGLE_BUTTON(widget))
    return true;
  reinterpret_cast<AccessibilityEventRouter *>(user_data)
      ->DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
  return true;
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
  reinterpret_cast<AccessibilityEventRouter *>(user_data)
      ->DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
  return true;
}

gboolean OnSwitchPage(GSignalInvocationHint *ihint,
                      guint n_param_values,
                      const GValue* param_values,
                      gpointer user_data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(param_values));
  reinterpret_cast<AccessibilityEventRouter *>(user_data)
      ->DispatchAccessibilityNotification(
          widget, NotificationType::ACCESSIBILITY_CONTROL_ACTION);
  return true;
}

}  // anonymous namespace

AccessibilityEventRouter::AccessibilityEventRouter() {
  // We don't want our event listeners to be installed if accessibility is
  // disabled. Install listeners so we can install and uninstall them as
  // needed, then install them now if it's currently enabled.
  ExtensionAccessibilityEventRouter *accessibility_event_router =
      ExtensionAccessibilityEventRouter::GetInstance();
  accessibility_event_router->AddOnEnabledListener(
      NewCallback(this,
                  &AccessibilityEventRouter::InstallEventListeners));
  accessibility_event_router->AddOnDisabledListener(
      NewCallback(this,
                  &AccessibilityEventRouter::RemoveEventListeners));
  if (accessibility_event_router->IsAccessibilityEnabled()) {
    InstallEventListeners();
  }
}

// static
AccessibilityEventRouter* AccessibilityEventRouter::GetInstance() {
  return Singleton<AccessibilityEventRouter>::get();
}

void AccessibilityEventRouter::InstallEventListeners() {
  // Create and destroy a GtkNotebook to ensure this module is loaded,
  // otherwise we can't lookup its signals.  All of the other modules we
  // need will already be loaded by the time we get here.
  g_object_unref(g_object_ref_sink(gtk_notebook_new()));

  // Add signal emission hooks for the events we're interested in.
  focus_hook_ = g_signal_add_emission_hook(
      g_signal_lookup("focus-in-event", GTK_TYPE_WIDGET),
      0, OnWidgetFocused, (gpointer)this, NULL);
  click_hook_ = g_signal_add_emission_hook(
      g_signal_lookup("clicked", GTK_TYPE_BUTTON),
      0, OnButtonClicked, (gpointer)this, NULL);
  toggle_hook_ = g_signal_add_emission_hook(
      g_signal_lookup("toggled", GTK_TYPE_TOGGLE_BUTTON),
      0, OnButtonToggled, (gpointer)this, NULL);
  switch_page_hook_ = g_signal_add_emission_hook(
      g_signal_lookup("switch-page", GTK_TYPE_NOTEBOOK),
      0, OnSwitchPage, (gpointer)this, NULL);
}

void AccessibilityEventRouter::RemoveEventListeners() {
  g_signal_remove_emission_hook(
      g_signal_lookup("focus-in-event", GTK_TYPE_WIDGET), focus_hook_);
  g_signal_remove_emission_hook(
      g_signal_lookup("clicked", GTK_TYPE_BUTTON), click_hook_);
  g_signal_remove_emission_hook(
      g_signal_lookup("toggled", GTK_TYPE_TOGGLE_BUTTON), toggle_hook_);
  g_signal_remove_emission_hook(
      g_signal_lookup("switch-page", GTK_TYPE_NOTEBOOK), switch_page_hook_);
}

void AccessibilityEventRouter::AddRootWidget(
    GtkWidget* root_widget, Profile* profile) {
  root_widget_profile_map_[root_widget] = profile;
}

void AccessibilityEventRouter::RemoveRootWidget(GtkWidget* root_widget) {
  DCHECK(root_widget_profile_map_.find(root_widget) !=
         root_widget_profile_map_.end());
  root_widget_profile_map_.erase(root_widget);
}

void AccessibilityEventRouter::IgnoreWidget(GtkWidget* widget) {
  widget_info_map_[widget].ignore = true;
}

void AccessibilityEventRouter::SetWidgetName(
    GtkWidget* widget, std::string name) {
  widget_info_map_[widget].name = name;
}

void AccessibilityEventRouter::RemoveWidget(GtkWidget* widget) {
  DCHECK(widget_info_map_.find(widget) != widget_info_map_.end());
  widget_info_map_.erase(widget);
}

bool AccessibilityEventRouter::IsWidgetAccessible(
    GtkWidget* widget, Profile** profile) {
  // First see if it's a descendant of a root widget.
  bool is_accessible = false;
  for (base::hash_map<GtkWidget*, Profile*>::const_iterator iter =
           root_widget_profile_map_.begin();
       iter != root_widget_profile_map_.end();
       ++iter) {
    if (gtk_widget_is_ancestor(widget, iter->first)) {
      is_accessible = true;
      if (profile)
        *profile = iter->second;
      break;
    }
  }
  if (!is_accessible)
    return false;

  // Now make sure it's not marked as a widget to be ignored.
  base::hash_map<GtkWidget*, WidgetInfo>::const_iterator iter =
      widget_info_map_.find(widget);
  if (iter != widget_info_map_.end() && iter->second.ignore) {
    is_accessible = false;
  }

  return is_accessible;
}

std::string AccessibilityEventRouter::GetWidgetName(GtkWidget* widget) {
  base::hash_map<GtkWidget*, WidgetInfo>::const_iterator iter =
      widget_info_map_.find(widget);
  if (iter != widget_info_map_.end()) {
    return iter->second.name;
  } else {
    return "";
  }
}

void AccessibilityEventRouter::DispatchAccessibilityNotification(
    GtkWidget* widget, NotificationType type) {
  Profile *profile;
  if (!IsWidgetAccessible(widget, &profile))
    return;

  // The order of these checks matters, because, for example, a radio button
  // is a subclass of button.  We need to catch the most specific type that
  // we can handle for each object.
  if (GTK_IS_RADIO_BUTTON(widget)) {
    SendRadioButtonNotification(widget, type, profile);
  } else if (GTK_IS_TOGGLE_BUTTON(widget)) {
    SendCheckboxNotification(widget, type, profile);
  } else if (GTK_IS_BUTTON(widget)) {
    SendButtonNotification(widget, type, profile);
  } else if (GTK_IS_ENTRY(widget)) {
    SendTextBoxNotification(widget, type, profile);
  } else if (GTK_IS_NOTEBOOK(widget)) {
    SendTabNotification(widget, type, profile);
  }
}

void AccessibilityEventRouter::SendRadioButtonNotification(
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

void AccessibilityEventRouter::SendCheckboxNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  std::string button_name = GetWidgetName(widget);
  if (button_name.empty() && gtk_button_get_label(GTK_BUTTON(widget)))
    button_name = gtk_button_get_label(GTK_BUTTON(widget));
  bool checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  AccessibilityCheckboxInfo info(profile, button_name, checked);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouter::SendButtonNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  std::string button_name = GetWidgetName(widget);
  if (button_name.empty() && gtk_button_get_label(GTK_BUTTON(widget)))
    button_name = gtk_button_get_label(GTK_BUTTON(widget));
  AccessibilityButtonInfo info(profile, button_name);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouter::SendTextBoxNotification(
    GtkWidget* widget, NotificationType type, Profile* profile) {
  std::string name = GetWidgetName(widget);
  std::string value = gtk_entry_get_text(GTK_ENTRY(widget));
  gint start_pos;
  gint end_pos;
  gtk_editable_get_selection_bounds(GTK_EDITABLE(widget), &start_pos, &end_pos);
  AccessibilityTextBoxInfo info(profile, name, false);
  info.SetValue(value, start_pos, end_pos);
  SendAccessibilityNotification(type, &info);
}

void AccessibilityEventRouter::SendTabNotification(
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
