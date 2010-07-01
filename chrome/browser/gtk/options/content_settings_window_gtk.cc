// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/content_settings_window_gtk.h"

#include <string>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

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
      geolocation_page_(profile, CONTENT_SETTINGS_TYPE_GEOLOCATION) {
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

  notebook_ = gtk_notebook_new();

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
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kNotebookPages); ++i) {
    std::string label = l10n_util::GetStringUTF8(kNotebookPages[i].message_id);
    // Since the tabs are on the side, add some padding space to the label.
    gtk_notebook_append_page(
        GTK_NOTEBOOK(notebook_),
        kNotebookPages[i].widget,
        gtk_label_new((" " + label + " ").c_str()));
  }

  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook_), GTK_POS_LEFT);

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), notebook_);

  // TODO(thakis): Get rid of |+ 1| once the notifcations pane is done.
  DCHECK_EQ(gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)) + 1,
            CONTENT_SETTINGS_NUM_TYPES);

  // Need to show the notebook before connecting switch-page signal, otherwise
  // we'll immediately get a signal switching to page 0 and overwrite our
  // last_selected_page_ value.
  gtk_util::ShowDialogWithLocalizedSize(dialog_, -1, -1, true);

  g_signal_connect(notebook_, "switch-page",
                   G_CALLBACK(OnSwitchPageThunk), this);

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

  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), page);
}

void ContentSettingsWindowGtk::OnSwitchPage(
    GtkWidget* notebook,
    GtkNotebookPage* page,
    guint page_num) {
  int index = page_num;
  DCHECK(index > CONTENT_SETTINGS_TYPE_DEFAULT &&
         index < CONTENT_SETTINGS_NUM_TYPES);
  last_selected_page_.SetValue(index);
}

void ContentSettingsWindowGtk::OnWindowDestroy(GtkWidget* widget) {
  settings_window = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}
