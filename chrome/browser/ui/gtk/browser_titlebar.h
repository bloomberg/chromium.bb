// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that contains the gtk widgets that make up the titlebar.  The
// titlebar consists of the tabstrip and if the custom chrome frame is turned
// on, it includes the taller titlebar and minimize, restore, maximize, and
// close buttons.

#ifndef CHROME_BROWSER_UI_GTK_BROWSER_TITLEBAR_H_
#define CHROME_BROWSER_UI_GTK_BROWSER_TITLEBAR_H_

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/ui/gtk/titlebar_throb_animation.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/x/active_window_watcher_x_observer.h"

class AvatarMenuButtonGtk;
class BrowserWindowGtk;
class CustomDrawButton;
class GtkThemeService;
class MenuGtk;
class PopupPageMenuModel;

namespace content {
class WebContents;
}

class BrowserTitlebar : public content::NotificationObserver,
                        public ui::ActiveWindowWatcherXObserver,
                        public ui::SimpleMenuModel::Delegate {
 public:
  // A default button order string for when we aren't asking gconf for the
  // metacity configuration.
  static const char kDefaultButtonString[];

  BrowserTitlebar(BrowserWindowGtk* browser_window, GtkWindow* window);
  virtual ~BrowserTitlebar();

  // Updates the title and icon when in app or popup mode (no tabstrip).
  void UpdateTitleAndIcon();

  GtkWidget* widget() {
    return container_;
  }

  void set_window(GtkWindow* window) { window_ = window; }

  // Build the titlebar, the space above the tab strip, and (maybe) the min,
  // max, close buttons. |container_| is the gtk container that we put the
  // widget into.
  void Init();

  // Builds the buttons based on the metacity |button_string|.
  void BuildButtons(const std::string& button_string);

  // Update the appearance of the title bar based on whether we're showing a
  // custom frame or not. If |use_custom_frame| is true, we show an extra
  // tall titlebar and the min/max/close buttons.
  void UpdateCustomFrame(bool use_custom_frame);

  // Called by the browser asking us to update the loading throbber.
  // |web_contents| is the tab that is associated with the window throbber.
  // |web_contents| can be null.
  void UpdateThrobber(content::WebContents* web_contents);

  // On Windows, right clicking in the titlebar background brings up the system
  // menu. There's no such thing on linux, so we just show the menu items we
  // add to the menu.
  void ShowContextMenu(GdkEventButton* event);

  AvatarMenuButtonGtk* avatar_button() { return avatar_button_.get(); }

 private:
  class ContextMenuModel : public ui::SimpleMenuModel {
   public:
    explicit ContextMenuModel(ui::SimpleMenuModel::Delegate* delegate);
  };

  // Builds the button as denoted by |button_token|. Returns true if the button
  // is created successfully.
  bool BuildButton(const std::string& button_token, bool left_side);

  // Retrieves the 3 image ids (IDR_) and a tooltip id (IDS) for the purpose of
  // painting a CustomDraw button.
  void GetButtonResources(const std::string& button_name,
                          int* normal_image_id,
                          int* pressed_image_id,
                          int* hover_image_id,
                          int* tooltip_id) const;

  // Constructs a CustomDraw button given button name and left or right side of
  // the titlebar where the button is placed.
  CustomDrawButton* CreateTitlebarButton(const std::string& button_name,
                                         bool left_side);

  // Lazily builds and returns |titlebar_{left,right}_buttons_vbox_| and their
  // subtrees. We do this lazily because in most situations, only one of them
  // is allocated (though the user can (and do) manually mess with their gconf
  // settings to get absolutely horrid combinations of buttons on both sides.
  GtkWidget* GetButtonHBox(bool left_side);

  // Updates the theme supplied background color and image.
  void UpdateButtonBackground(CustomDrawButton* button);

  // Updates the color of the title bar. Called whenever we have a state
  // change in the window.
  void UpdateTextColor();

  // Update the titlebar spacing based on the custom frame and maximized state.
  void UpdateTitlebarAlignment();

  // Updates the avatar image displayed, either a multi-profile avatar or the
  // incognito spy guy.
  void UpdateAvatar();

  // The maximize button was clicked, take an action depending on which mouse
  // button the user pressed.
  void MaximizeButtonClicked();

  // Updates the visibility of the maximize and restore buttons; only one can
  // be visible at a time.
  void UpdateMaximizeRestoreVisibility();

  // Callback for changes to window state.  This includes
  // maximizing/restoring/minimizing the window.
  CHROMEG_CALLBACK_1(BrowserTitlebar, gboolean, OnWindowStateChanged,
                     GtkWindow*, GdkEventWindowState*);

  // Callback for mousewheel events.
  CHROMEGTK_CALLBACK_1(BrowserTitlebar, gboolean, OnScroll,
                       GdkEventScroll*);

  // Callback for min/max/close buttons.
  CHROMEGTK_CALLBACK_0(BrowserTitlebar, void, OnButtonClicked);

  // Callback for favicon/settings buttons.
  CHROMEGTK_CALLBACK_1(BrowserTitlebar, gboolean,
                       OnFaviconMenuButtonPressed, GdkEventButton*);

  // -- Context Menu -----------------------------------------------------------

  // SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overriden from ActiveWindowWatcherXObserver.
  virtual void ActiveWindowChanged(GdkWindow* active_window) OVERRIDE;

  // Whether to display the avatar image.
  bool ShouldDisplayAvatar();

  // Returns true if the profile associated with this BrowserWindow is off the
  // record.
  bool IsOffTheRecord();

  // Pointers to the browser window that owns us and its GtkWindow.
  BrowserWindowGtk* browser_window_;
  GtkWindow* window_;

  // The container widget the holds the hbox which contains the whole titlebar.
  GtkWidget* container_;

  // The hbox that contains the whole titlebar.
  GtkWidget* container_hbox_;

  // VBoxes that holds the min/max/close buttons box and an optional padding
  // that defines the skyline if the user turns off window manager decorations.
  // There is a left and right version of this box since we try to integrate
  // with the recent Ubuntu theme changes which put the buttons on the left.
  GtkWidget* titlebar_left_buttons_vbox_;
  GtkWidget* titlebar_right_buttons_vbox_;

  // HBoxes that contains the actual min/max/close buttons.
  GtkWidget* titlebar_left_buttons_hbox_;
  GtkWidget* titlebar_right_buttons_hbox_;

  // Avatar frame. One of these frames holds either the spy guy in incognito
  // mode, or the avatar image when using multiple-profiles. It's the side with
  // the least window control buttons (close, maximize, minimize).
  // These pointers are never NULL.
  GtkWidget* titlebar_left_avatar_frame_;
  GtkWidget* titlebar_right_avatar_frame_;

  // The avatar image widget. It will be NULL if there is no avatar displayed.
  GtkWidget* avatar_;

  // Padding between the titlebar buttons and the top of the screen. Only show
  // when not maximized.
  GtkWidget* top_padding_left_;
  GtkWidget* top_padding_right_;

  // Gtk alignment that contains the tab strip.  If the user turns off window
  // manager decorations, we draw this taller.
  GtkWidget* titlebar_alignment_;

  // The favicon and page title used when in app mode or popup mode.
  GtkWidget* app_mode_favicon_;
  GtkWidget* app_mode_title_;

  // Whether we are using a custom frame.
  bool using_custom_frame_;

  // Whether we have focus (gtk_window_is_active() sometimes returns the wrong
  // value, so manually track the focus-in and focus-out events.)
  bool window_has_focus_;

  // Whether to display the avatar image on the left or right of the titlebar.
  bool display_avatar_on_left_;

  // We change the size of these three buttons when the window is maximized, so
  // we use these structs to keep track of their original size.
  GtkRequisition close_button_req_;
  GtkRequisition minimize_button_req_;
  GtkRequisition restore_button_req_;

  // Maximize and restore widgets in the titlebar.
  scoped_ptr<CustomDrawButton> minimize_button_;
  scoped_ptr<CustomDrawButton> maximize_button_;
  scoped_ptr<CustomDrawButton> restore_button_;
  scoped_ptr<CustomDrawButton> close_button_;

  // The context menu view and model.
  scoped_ptr<MenuGtk> context_menu_;
  scoped_ptr<ContextMenuModel> context_menu_model_;

  // The favicon menu view and model.
  scoped_ptr<MenuGtk> favicon_menu_;
  scoped_ptr<PopupPageMenuModel> favicon_menu_model_;

  // The throbber used when the window is in app mode or popup window mode.
  TitlebarThrobAnimation throbber_;

  // The avatar button.
  scoped_ptr<AvatarMenuButtonGtk> avatar_button_;

  // Theme provider for building buttons.
  GtkThemeService* theme_service_;

  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_UI_GTK_BROWSER_TITLEBAR_H_
