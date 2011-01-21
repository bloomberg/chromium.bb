// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/content_settings_window_gtk.h"

#include <string>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The singleton options window object.
ContentSettingsWindowGtk* settings_window = NULL;

}  // namespace

// static
void ContentSettingsWindowGtk::Show(GtkWindow* parent,
                                    ContentSettingsType page,
                                    Profile* profile) {
  DCHECK(profile);

  if (!settings_window) {
    // Creating and initializing a bunch of controls generates a bunch of
    // spurious events as control values change. Temporarily suppress
    // accessibility events until the window is created.
    profile->PauseAccessibilityEvents();

    // Create the options window.
    settings_window = new ContentSettingsWindowGtk(parent, profile);

    // Resume accessibility events.
    profile->ResumeAccessibilityEvents();
  }
  settings_window->ShowContentSettingsTab(page);
}

ContentSettingsWindowGtk::ContentSettingsWindowGtk(GtkWindow* parent,
                                                   Profile* profile)
    : profile_(profile),
      cookie_page_(profile),
      image_page_(profile, CONTENT_SETTINGS_TYPE_IMAGES),
      javascript_page_(profile, CONTENT_SETTINGS_TYPE_JAVASCRIPT),
      plugin_page_(profile, CONTENT_SETTINGS_TYPE_PLUGINS),
      popup_page_(profile, CONTENT_SETTINGS_TYPE_POPUPS),
      geolocation_page_(profile, CONTENT_SETTINGS_TYPE_GEOLOCATION),
      notifications_page_(profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
  const struct {
    int message_id;
    GtkWidget* widget;
  } kNotebookPages[] = {
    { IDS_COOKIES_TAB_LABEL, cookie_page_.get_page_widget() },
    { IDS_IMAGES_TAB_LABEL, image_page_.get_page_widget() },
    { IDS_JAVASCRIPT_TAB_LABEL, javascript_page_.get_page_widget() },
    { IDS_PLUGIN_TAB_LABEL, plugin_page_.get_page_widget() },
    { IDS_POPUP_TAB_LABEL, popup_page_.get_page_widget() },
    { IDS_GEOLOCATION_TAB_LABEL, geolocation_page_.get_page_widget() },
    { IDS_NOTIFICATIONS_TAB_LABEL, notifications_page_.get_page_widget() },
  };

  // We don't need to observe changes in this value.
  last_selected_page_.Init(prefs::kContentSettingsWindowLastTabIndex,
                           profile->GetPrefs(), NULL);

  std::string dialog_name = l10n_util::GetStringUTF8(
      IDS_CONTENT_SETTINGS_TITLE);
  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_window_set_policy(GTK_WINDOW(dialog_), FALSE, FALSE, TRUE);

  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile_));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, -1);
  // Allow browser windows to go in front of the options dialog in metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  gtk_util::SetWindowIcon(GTK_WINDOW(dialog_));

  // Create hbox with list view and notebook.
  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kContentAreaSpacing);

  list_ = gtk_tree_view_new();
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list_), FALSE);

  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(
      "List Item", renderer, "text", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(list_), column);

  const int kColumnCount = 1;
  GtkListStore* store = gtk_list_store_new(kColumnCount, G_TYPE_STRING);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kNotebookPages); ++i) {
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    std::string label = l10n_util::GetStringUTF8(kNotebookPages[i].message_id);
    gtk_list_store_set(store, &iter, 0, label.c_str(), -1);
  }
  gtk_tree_view_set_model(GTK_TREE_VIEW(list_), GTK_TREE_MODEL(store));
  g_object_unref(store);

  // Needs to happen after the model is all set up.
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(list_));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

  // Wrap the list widget in a scrolled window in order to have a frame.
  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_container_add(GTK_CONTAINER(scrolled), list_);
  gtk_box_pack_start(GTK_BOX(hbox), scrolled, FALSE, FALSE, 0);

  notebook_ = gtk_notebook_new();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kNotebookPages); ++i) {
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_),
                             kNotebookPages[i].widget,
                             NULL);
  }
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook_), FALSE);
  gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook_), FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), notebook_, FALSE, FALSE, 0);
  DCHECK_EQ(gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)),
            CONTENT_SETTINGS_NUM_TYPES);

  // Create vbox with "Features:" text and hbox below.
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_CONTENT_SETTINGS_FEATURES_LABEL).c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), vbox);

  // Need to show the notebook before connecting switch-page signal, otherwise
  // we'll immediately get a signal switching to page 0 and overwrite our
  // last_selected_page_ value.
  gtk_util::ShowDialogWithLocalizedSize(dialog_, -1, -1, true);

  g_signal_connect(notebook_, "switch-page",
                   G_CALLBACK(OnSwitchPageThunk), this);
  g_signal_connect(selection, "changed",
                   G_CALLBACK(OnListSelectionChangedThunk), this);

  // We only have one button and don't do any special handling, so just hook it
  // directly to gtk_widget_destroy.
  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);

  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroyThunk), this);
}

ContentSettingsWindowGtk::~ContentSettingsWindowGtk() {
}

void ContentSettingsWindowGtk::ShowContentSettingsTab(
    ContentSettingsType page) {
  // Bring options window to front if it already existed and isn't already
  // in front
  gtk_util::PresentWindow(dialog_,  gtk_get_current_event_time());

  if (page == CONTENT_SETTINGS_TYPE_DEFAULT) {
    // Remember the last visited page from local state.
    page = static_cast<ContentSettingsType>(last_selected_page_.GetValue());
    if (page == CONTENT_SETTINGS_TYPE_DEFAULT)
      page = CONTENT_SETTINGS_TYPE_COOKIES;
  }
  // If the page number is out of bounds, reset to the first tab.
  if (page < 0 || page >= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)))
    page = CONTENT_SETTINGS_TYPE_COOKIES;

  gtk_tree::SelectAndFocusRowNum(page, GTK_TREE_VIEW(list_));
}

void ContentSettingsWindowGtk::OnSwitchPage(
    GtkWidget* notebook,
    GtkNotebookPage* page,
    guint page_num) {
  int index = page_num;
  DCHECK(index > CONTENT_SETTINGS_TYPE_DEFAULT &&
         index < CONTENT_SETTINGS_NUM_TYPES);

  // Keep list in sync.
  GtkTreeModel* model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected(
      gtk_tree_view_get_selection(GTK_TREE_VIEW(list_)), &model, &iter)) {
    gint row_index = gtk_tree::GetRowNumForIter(model, &iter);
    if (row_index == index)
      return;
  }
  gtk_tree::SelectAndFocusRowNum(index, GTK_TREE_VIEW(list_));
}

void ContentSettingsWindowGtk::OnWindowDestroy(GtkWidget* widget) {
  settings_window = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void ContentSettingsWindowGtk::OnListSelectionChanged(
    GtkTreeSelection* selection) {
  GtkTreeModel* model;
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
    NOTREACHED();
    return;
  }
  gint row_index = gtk_tree::GetRowNumForIter(model, &iter);
  DCHECK(row_index > CONTENT_SETTINGS_TYPE_DEFAULT &&
         row_index < CONTENT_SETTINGS_NUM_TYPES);

  last_selected_page_.SetValue(row_index);

  if (row_index != gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_)))
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), row_index);
}
