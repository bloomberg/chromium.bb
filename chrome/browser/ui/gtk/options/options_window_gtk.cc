// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/options/advanced_page_gtk.h"
#include "chrome/browser/ui/gtk/options/content_page_gtk.h"
#include "chrome/browser/ui/gtk/options/general_page_gtk.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/options/internet_page_view.h"
#include "chrome/browser/chromeos/options/system_page_view.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowGtk
//
// The contents of the Options dialog window.

class OptionsWindowGtk {
 public:
  explicit OptionsWindowGtk(Profile* profile);
  ~OptionsWindowGtk();

  // Shows the Tab corresponding to the specified OptionsPage.
  void ShowOptionsPage(OptionsPage page, OptionsGroup highlight_group);

 private:
  static void OnSwitchPage(GtkNotebook* notebook, GtkNotebookPage* page,
                           guint page_num, OptionsWindowGtk* window);

  static void OnWindowDestroy(GtkWidget* widget, OptionsWindowGtk* window);

  // The options dialog.
  GtkWidget* dialog_;

  // The container of the option pages.
  GtkWidget* notebook_;

  // The Profile associated with these options.
  Profile* profile_;

  // The general page.
  GeneralPageGtk general_page_;

  // The content page.
  ContentPageGtk content_page_;

  // The advanced (user data) page.
  AdvancedPageGtk advanced_page_;

  // The last page the user was on when they opened the Options window.
  IntegerPrefMember last_selected_page_;

  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(OptionsWindowGtk);
};

// The singleton options window object.
static OptionsWindowGtk* options_window = NULL;

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowGtk, public:

OptionsWindowGtk::OptionsWindowGtk(Profile* profile)
      // Always show preferences for the original profile. Most state when off
      // the record comes from the original profile, but we explicitly use
      // the original profile to avoid potential problems.
    : profile_(profile->GetOriginalProfile()),
      general_page_(profile_),
      content_page_(profile_),
      advanced_page_(profile_) {

  // We don't need to observe changes in this value.
  last_selected_page_.Init(prefs::kOptionsWindowLastTabIndex,
                           g_browser_process->local_state(), NULL);

  std::string dialog_name =
      l10n_util::GetStringFUTF8(
          IDS_PREFERENCES_DIALOG_TITLE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      // Prefs window is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);

  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, -1);
  // Allow browser windows to go in front of the options dialog in metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  notebook_ = gtk_notebook_new();

#if defined(OS_CHROMEOS)
  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      (new chromeos::SystemPageView(profile_))->WrapInGtkWidget(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_OPTIONS_SYSTEM_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      (new chromeos::InternetPageView(profile_))->WrapInGtkWidget(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_OPTIONS_INTERNET_TAB_LABEL).c_str()));
#endif

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      general_page_.get_page_widget(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_OPTIONS_GENERAL_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      content_page_.get_page_widget(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_OPTIONS_CONTENT_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      advanced_page_.get_page_widget(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_TAB_LABEL).c_str()));

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), notebook_);

  DCHECK_EQ(
      gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)), OPTIONS_PAGE_COUNT);

  // Show the content so that we can compute full dialog size, both
  // for centering and because we want to show the notebook before
  // connecting switch-page signal, otherwise we'll immediately get a
  // signal switching to page 0 and overwrite our last_selected_page_
  // value.
  gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(dialog_)));

  if (Browser* b = BrowserList::GetLastActive()) {
    gtk_util::CenterOverWindow(GTK_WINDOW(dialog_),
                               b->window()->GetNativeHandle());
  }

  // Now that we're centered over the browser, we add our dialog to its own
  // window group. We don't do anything with the response and we don't want the
  // options window's modal dialogs to be associated with the main browser
  // window because gtk grabs work on a per window group basis.
  gtk_window_group_add_window(gtk_window_group_new(), GTK_WINDOW(dialog_));
  g_object_unref(gtk_window_get_group(GTK_WINDOW(dialog_)));

  g_signal_connect(notebook_, "switch-page", G_CALLBACK(OnSwitchPage), this);

  // We only have one button and don't do any special handling, so just hook it
  // directly to gtk_widget_destroy.
  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);

  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);

  gtk_widget_show(dialog_);
}

OptionsWindowGtk::~OptionsWindowGtk() {
}

void OptionsWindowGtk::ShowOptionsPage(OptionsPage page,
                                       OptionsGroup highlight_group) {
  if (Browser* b = BrowserList::GetLastActive()) {
    gtk_util::CenterOverWindow(GTK_WINDOW(dialog_),
                               b->window()->GetNativeHandle());
  }

  // Bring options window to front if it already existed and isn't already
  // in front
  gtk_window_present_with_time(GTK_WINDOW(dialog_),
                               gtk_get_current_event_time());

  if (page == OPTIONS_PAGE_DEFAULT) {
    // Remember the last visited page from local state.
    page = static_cast<OptionsPage>(last_selected_page_.GetValue());
    if (page == OPTIONS_PAGE_DEFAULT)
      page = OPTIONS_PAGE_GENERAL;
  }
  // If the page number is out of bounds, reset to the first tab.
  if (page < 0 || page >= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)))
    page = OPTIONS_PAGE_GENERAL;

  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), page);

  // TODO(mattm): set highlight_group
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowGtk, private:

// static
void OptionsWindowGtk::OnSwitchPage(GtkNotebook* notebook,
                                    GtkNotebookPage* page,
                                    guint page_num,
                                    OptionsWindowGtk* window) {
  int index = page_num;
  DCHECK(index > OPTIONS_PAGE_DEFAULT && index < OPTIONS_PAGE_COUNT);
  window->last_selected_page_.SetValue(index);
}

// static
void OptionsWindowGtk::OnWindowDestroy(GtkWidget* widget,
                                       OptionsWindowGtk* window) {
  options_window = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

#if !defined(OS_CHROMEOS)
// ShowOptionsWindow for non ChromeOS build. For ChromeOS build, see
// chrome/browser/chromeos/options/options_window_view.h
void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile) {
  DCHECK(profile);

  // If there's already an existing options window, activate it and switch to
  // the specified page.
  if (!options_window) {
    // Creating and initializing a bunch of controls generates a bunch of
    // spurious events as control values change. Temporarily suppress
    // accessibility events until the window is created.
    profile->PauseAccessibilityEvents();

    // Create the options window.
    options_window = new OptionsWindowGtk(profile);

    // Resume accessibility events.
    profile->ResumeAccessibilityEvents();
  }
  options_window->ShowOptionsPage(page, highlight_group);
}
#endif  // !defined(OS_CHROMEOS)
