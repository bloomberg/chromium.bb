// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_TOOLBAR_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_TOOLBAR_GTK_H_

#include <gtk/gtk.h>
#include <string>

#include "app/gtk_signal.h"
#include "app/menus/simple_menu_model.h"
#include "app/throb_animation.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/app_menu_model.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/menu_bar_helper.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/page_menu_model.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/wrench_menu_model.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"

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
                          public menus::SimpleMenuModel::Delegate,
                          public MenuGtk::Delegate,
                          public NotificationObserver,
                          public MenuBarHelper::Delegate,
                          public AnimationDelegate {
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

  GtkWidget* GetAppMenuButton() { return app_menu_button_.get(); }

  BrowserActionsToolbarGtk* GetBrowserActionsToolbar() {
    return actions_toolbar_.get();
  }

  LocationBarViewGtk* GetLocationBarView() { return location_bar_.get(); }

  bool collapsed() const { return collapsed_; }
  void set_collapsed(bool val);

  // We have to show padding on the bottom of the toolbar when the bookmark
  // is in floating mode. Otherwise the bookmark bar will paint it for us.
  void UpdateForBookmarkBarVisibility(bool show_bottom_padding);

  void ShowPageMenu();
  void ShowAppMenu();

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Overridden from MenuGtk::Delegate:
  virtual void StoppedShowing();
  virtual GtkIconSet* GetIconSetForId(int idr);

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdEnabled(int id) const;
  virtual bool IsCommandIdChecked(int id) const;
  virtual void ExecuteCommand(int id);
  virtual bool GetAcceleratorForCommandId(int id,
                                          menus::Accelerator* accelerator);

  // NotificationObserver implementation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  Profile* profile() { return profile_; }
  void SetProfile(Profile* profile);

  // Message that we should react to a state change.
  void UpdateTabContents(TabContents* contents, bool should_restore_state);

  // MenuBarHelper::Delegate implementation ------------------------------------
  virtual void PopupForButton(GtkWidget* button);
  virtual void PopupForButtonNextTo(GtkWidget* button,
                                    GtkMenuDirectionType dir);

  // AnimationDelegate implementation ------------------------------------------
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);

 private:
  // Builds a toolbar button with all the properties set.
  // |spacing| is the width of padding (in pixels) on the left and right of the
  // button.
  CustomDrawButton* BuildToolbarButton(int normal_id,
                                       int active_id,
                                       int highlight_id,
                                       int depressed_id,
                                       int background_id,
                                       const std::string& localized_tooltip,
                                       const char* stock_id,
                                       int spacing);

  // Create a menu for the toolbar given the icon id and tooltip.  Returns the
  // widget created.
  GtkWidget* BuildToolbarMenuButton(const std::string& localized_tooltip,
                                    OwnedWidgetGtk* owner);

  // Connect/Disconnect signals for dragging a url onto the home button.
  void SetUpDragForHomeButton(bool enable);

  // Helper for the PageAppMenu event handlers. Pops down the currently active
  // meun and pops up the other menu.
  void ChangeActiveMenu(GtkWidget* active_menu, guint timestamp);

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

  // Used to stop the upgrade notification animation.
  CHROMEGTK_CALLBACK_0(BrowserToolbarGtk, void, OnAppMenuShow);

  // Used to draw the upgrade notification badge.
  CHROMEGTK_CALLBACK_1(BrowserToolbarGtk, gboolean, OnAppMenuImageExpose,
                       GdkEventExpose*);

  // Updates preference-dependent state.
  void NotifyPrefChanged(const std::wstring* pref);

  // Start the upgrade notification animation.
  void ShowUpgradeReminder();

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
  GtkWidget* toolbar_right_;

  // Contains all the widgets of the location bar.
  GtkWidget* location_hbox_;

  // The location bar view.
  scoped_ptr<LocationBarViewGtk> location_bar_;

  // All the buttons in the toolbar.
  scoped_ptr<BackForwardButtonGtk> back_, forward_;
  scoped_ptr<CustomDrawButton> home_;
  scoped_ptr<ReloadButtonGtk> reload_;
  scoped_ptr<BrowserActionsToolbarGtk> actions_toolbar_;
  OwnedWidgetGtk page_menu_button_, app_menu_button_;

  // Keep a pointer to the menu button images because we change them when
  // the theme changes.
  GtkWidget* page_menu_image_;
  OwnedWidgetGtk app_menu_image_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  GtkThemeProvider* theme_provider_;

  scoped_ptr<MenuGtk> page_menu_;
  scoped_ptr<MenuGtk> app_menu_;

  PageMenuModel page_menu_model_;
  AppMenuModel app_menu_model_;
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

  MenuBarHelper menu_bar_helper_;

  // Manages the home button drop signal handler.
  scoped_ptr<GtkSignalRegistrar> drop_handler_;

  ThrobAnimation upgrade_reminder_animation_;

  // When collapsed, the toolbar is just a tiny strip, no controls are visible.
  bool collapsed_;

  DISALLOW_COPY_AND_ASSIGN(BrowserToolbarGtk);
};

#endif  // CHROME_BROWSER_GTK_BROWSER_TOOLBAR_GTK_H_
