// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BROWSER_TOOLBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_BROWSER_TOOLBAR_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/models/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

class BackForwardButtonGtk;
class Browser;
class BrowserActionsToolbarGtk;
class BrowserWindowGtk;
class CustomDrawButton;
class GtkThemeProvider;
class LocationBar;
class LocationBarViewGtk;
class Profile;
class ReloadButtonGtk;
class TabContents;
class ToolbarModel;

// View class that displays the GTK version of the toolbar and routes gtk
// events back to the Browser.
class BrowserToolbarGtk : public CommandUpdater::CommandObserver,
                          public ui::AcceleratorProvider,
                          public MenuGtk::Delegate,
                          public NotificationObserver {
 public:
  explicit BrowserToolbarGtk(Browser* browser, BrowserWindowGtk* window);
  virtual ~BrowserToolbarGtk();

  // Create the contents of the toolbar. |top_level_window| is the GtkWindow
  // to which we attach our accelerators.
  void Init(Profile* profile, GtkWindow* top_level_window);

  // Set the various widgets' ViewIDs.
  void SetViewIDs();

  void Show();
  void Hide();

  // Getter for the containing widget.
  GtkWidget* widget() {
    return event_box_;
  }

  // Getter for associated browser object.
  Browser* browser() {
    return browser_;
  }

  virtual LocationBar* GetLocationBar() const;

  ReloadButtonGtk* GetReloadButton() { return reload_.get(); }

  GtkWidget* GetAppMenuButton() { return wrench_menu_button_->widget(); }

  BrowserActionsToolbarGtk* GetBrowserActionsToolbar() {
    return actions_toolbar_.get();
  }

  LocationBarViewGtk* GetLocationBarView() { return location_bar_.get(); }

  // We have to show padding on the bottom of the toolbar when the bookmark
  // is in floating mode. Otherwise the bookmark bar will paint it for us.
  void UpdateForBookmarkBarVisibility(bool show_bottom_padding);

  void ShowAppMenu();

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Overridden from MenuGtk::Delegate:
  virtual void StoppedShowing();
  virtual GtkIconSet* GetIconSetForId(int idr);
  virtual bool AlwaysShowIconForCmd(int command_id) const;

  // Overridden from ui::AcceleratorProvider:
  virtual bool GetAcceleratorForCommandId(int id,
                                          ui::Accelerator* accelerator);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile() { return profile_; }
  void SetProfile(Profile* profile);

  // Message that we should react to a state change.
  void UpdateTabContents(TabContents* contents, bool should_restore_state);

 private:
  // Connect/Disconnect signals for dragging a url onto the home button.
  void SetUpDragForHomeButton(bool enable);

  // Sets the top corners of the toolbar to rounded, or sets them to normal,
  // depending on the state of the browser window. Returns false if no action
  // was taken (the roundedness was already correct), true otherwise.
  bool UpdateRoundedness();

  // Gtk callback for the "expose-event" signal.
  // The alignment contains the toolbar.
  CHROMEGTK_CALLBACK_1(BrowserToolbarGtk, gboolean, OnAlignmentExpose,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(BrowserToolbarGtk, gboolean, OnLocationHboxExpose,
                       GdkEventExpose*);

  // Gtk callback for the "clicked" signal.
  CHROMEGTK_CALLBACK_0(BrowserToolbarGtk, void, OnButtonClick);

  // Gtk callback to intercept mouse clicks to the menu buttons.
  CHROMEGTK_CALLBACK_1(BrowserToolbarGtk, gboolean, OnMenuButtonPressEvent,
                       GdkEventButton*);

  // Used for drags onto home button.
  CHROMEGTK_CALLBACK_6(BrowserToolbarGtk, void, OnDragDataReceived,
                       GdkDragContext*, gint, gint, GtkSelectionData*,
                       guint, guint);

  // Used to draw the upgrade notification badge.
  CHROMEGTK_CALLBACK_1(BrowserToolbarGtk, gboolean, OnWrenchMenuButtonExpose,
                       GdkEventExpose*);

  // Updates preference-dependent state.
  void NotifyPrefChanged(const std::string* pref);

  static void SetSyncMenuLabel(GtkWidget* widget, gpointer userdata);

  // Sometimes we only want to show the location w/o the toolbar buttons (e.g.,
  // in a popup window).
  bool ShouldOnlyShowLocation() const;

  // An event box that holds |toolbar_|. We need the toolbar to have its own
  // GdkWindow when we use the GTK drawing because otherwise the color from our
  // parent GdkWindow will leak through with some theme engines (such as
  // Clearlooks).
  GtkWidget* event_box_;

  // This widget handles padding around the outside of the toolbar.
  GtkWidget* alignment_;

  // Gtk widgets. The toolbar is an hbox with each of the other pieces of the
  // toolbar placed side by side.
  GtkWidget* toolbar_;

  // All widgets to the left or right of the |location_hbox_|. We put the
  // widgets on either side of location_hbox_ in their own toolbar so we can
  // set their minimum sizes independently of |location_hbox_| which needs to
  // grow/shrink in GTK+ mode.
  GtkWidget* toolbar_left_;

  // Contains all the widgets of the location bar.
  GtkWidget* location_hbox_;

  // The location bar view.
  scoped_ptr<LocationBarViewGtk> location_bar_;

  // All the buttons in the toolbar.
  scoped_ptr<BackForwardButtonGtk> back_, forward_;
  scoped_ptr<CustomDrawButton> home_;
  scoped_ptr<ReloadButtonGtk> reload_;
  scoped_ptr<BrowserActionsToolbarGtk> actions_toolbar_;
  scoped_ptr<CustomDrawButton> wrench_menu_button_;

  // The image shown in GTK+ mode in the wrench button.
  GtkWidget* wrench_menu_image_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  GtkThemeProvider* theme_provider_;

  scoped_ptr<MenuGtk> wrench_menu_;

  WrenchMenuModel wrench_menu_model_;

  Browser* browser_;
  BrowserWindowGtk* window_;
  Profile* profile_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // Preferences controlling the configured home page.
  StringPrefMember home_page_;
  BooleanPrefMember home_page_is_new_tab_page_;

  NotificationRegistrar registrar_;

  // A GtkEntry that isn't part of the hierarchy. We keep this for native
  // rendering.
  OwnedWidgetGtk offscreen_entry_;

  // Manages the home button drop signal handler.
  scoped_ptr<ui::GtkSignalRegistrar> drop_handler_;

  DISALLOW_COPY_AND_ASSIGN(BrowserToolbarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BROWSER_TOOLBAR_GTK_H_
