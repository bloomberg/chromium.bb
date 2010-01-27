// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/options_window.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/gtk/options/advanced_page_gtk.h"
#include "chrome/browser/gtk/options/content_page_gtk.h"
#include "chrome/browser/gtk/options/general_page_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/accessibility_events.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/internet_page_view.h"
#include "chrome/browser/chromeos/system_page_view.h"
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

  scoped_ptr<AccessibleWidgetHelper> accessibility_widget_helper_;

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

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringFUTF8(IDS_OPTIONS_DIALOG_TITLE,
          WideToUTF16(l10n_util::GetString(IDS_PRODUCT_NAME))).c_str(),
      // Prefs window is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, -1);
  // Allow browser windows to go in front of the options dialog in metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  accessibility_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile));

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

  if (Browser* b = BrowserList::GetLastActive()) {
    // Set temporary transient parent to align the windows before showing it.
    // gtk_widget_show_all below shows options dialog before ShowOptionsPage
    // is able to move the window to desired position. Therefor without
    // gtk_window_set_transient_for(), the window initially appears at 0,0
    // and then moves to the center. This trick eliminates blinking.
    gtk_window_set_transient_for(GTK_WINDOW(dialog_),
                                 b->window()->GetNativeHandle());
    gtk_window_set_position(GTK_WINDOW(dialog_), GTK_WIN_POS_CENTER_ON_PARENT);
  }

  // Need to show the notebook before connecting switch-page signal, otherwise
  // we'll immediately get a signal switching to page 0 and overwrite our
  // last_selected_page_ value.
  gtk_widget_show_all(dialog_);

  g_signal_connect(notebook_, "switch-page", G_CALLBACK(OnSwitchPage), this);

  // We only have one button and don't do any special handling, so just hook it
  // directly to gtk_widget_destroy.
  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);

  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);
}

OptionsWindowGtk::~OptionsWindowGtk() {
}

static gfx::Rect GetWindowBounds(GtkWindow* window) {
  gint x, y, width, height;
  gtk_window_get_position(window, &x, &y);
  gtk_window_get_size(window, &width, &height);
  return gfx::Rect(x, y, width, height);
}

static gfx::Size GetWindowSize(GtkWindow* window) {
  gint width, height;
  gtk_window_get_size(window, &width, &height);
  return gfx::Size(width, height);
}

void OptionsWindowGtk::ShowOptionsPage(OptionsPage page,
                                       OptionsGroup highlight_group) {
  // Remove transient parent to detach window since it doesn't belong to
  // any particular browser instance.
  gtk_window_set_transient_for(GTK_WINDOW(dialog_), NULL);
  if (Browser* b = BrowserList::GetLastActive()) {
    // Move dialog to user expected position.
    gfx::Rect frame_bounds = GetWindowBounds(b->window()->GetNativeHandle());
    gfx::Point origin = frame_bounds.origin();
    gfx::Size size = GetWindowSize(GTK_WINDOW(dialog_));
    origin.Offset(
        (frame_bounds.width() - size.width()) / 2,
        (frame_bounds.height() - size.height()) / 2);
    gtk_window_move(GTK_WINDOW(dialog_), origin.x(), origin.y());
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

  std::string name = l10n_util::GetStringFUTF8(
      IDS_OPTIONS_DIALOG_TITLE,
      WideToUTF16(l10n_util::GetString(IDS_PRODUCT_NAME)));
  AccessibilityWindowInfo info(profile, name);

  NotificationService::current()->Notify(
      NotificationType::ACCESSIBILITY_WINDOW_OPENED,
      Source<Profile>(profile),
      Details<AccessibilityWindowInfo>(&info));
}
