// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that contains the gtk widgets that make up the titlebar.  The
// titlebar consists of the tabstrip and if the custom chrome frame is turned
// on, it includes the taller titlebar and minimize, restore, maximize, and
// close buttons.

#ifndef CHROME_BROWSER_GTK_BROWSER_TITLEBAR_H_
#define CHROME_BROWSER_GTK_BROWSER_TITLEBAR_H_

#include <gtk/gtk.h>

#include "app/active_window_watcher_x.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class BrowserWindowGtk;
class CustomDrawButton;
class GtkThemeProvider;
class TabContents;
class TabStripGtk;

class BrowserTitlebar : public MenuGtk::Delegate,
                        public NotificationObserver,
                        public ActiveWindowWatcherX::Observer {
 public:
  BrowserTitlebar(BrowserWindowGtk* browser_window, GtkWindow* window);
  virtual ~BrowserTitlebar();

  GtkWidget* widget() {
    return container_;
  }

  void set_window(GtkWindow* window) { window_ = window; }

  // Update the appearance of the title bar based on whether we're showing a
  // custom frame or not.  If |use_custom_frame| is true, we show an extra
  // tall titlebar and the min/max/close buttons.
  void UpdateCustomFrame(bool use_custom_frame);

  // Updates the title and icon when in app or popup mode (no tabstrip).
  void UpdateTitleAndIcon();

  // Called by the browser asking us to update the loading throbber.
  // |tab_contents| is the tab that is associated with the window throbber.
  // |tab_contents| can be null.
  void UpdateThrobber(TabContents* tab_contents);

  // On Windows, right clicking in the titlebar background brings up the system
  // menu.  There's no such thing on linux, so we just show the menu items we
  // add to the menu.
  void ShowContextMenu();

 private:
  // A helper class to keep track of which frame of the throbber animation
  // we're showing.
  class Throbber {
   public:
    Throbber() : current_frame_(0), current_waiting_frame_(0) {}

    // Get the next frame in the animation. The image is owned by the throbber
    // so the caller doesn't need to unref.  |is_waiting| is true if we're
    // still waiting for a response.
    GdkPixbuf* GetNextFrame(bool is_waiting);

    // Reset back to the first frame.
    void Reset();
   private:
    // Make sure the frames are loaded.
    static void InitFrames();

    int current_frame_;
    int current_waiting_frame_;
  };

  // Build the titlebar, the space above the tab
  // strip, and (maybe) the min, max, close buttons.  |container| is the gtk
  // continer that we put the widget into.
  void Init();

  // Constructs a CustomDraw button given 3 image ids (IDR_), the box to place
  // the button into, and a tooltip id (IDS_).
  CustomDrawButton* BuildTitlebarButton(int image, int image_pressed,
                                        int image_hot, GtkWidget* box,
                                        int tooltip);

  // Update the titlebar spacing based on the custom frame and maximized state.
  void UpdateTitlebarAlignment();

  // Updates the color of the title bar. Called whenever we have a state
  // change in the window.
  void UpdateTextColor();

  // Show the menu that the user gets from left-clicking the favicon.
  void ShowFaviconMenu(GdkEventButton* event);

  // The maximize button was clicked, take an action depending on which mouse
  // button the user pressed.
  void MaximizeButtonClicked();

  // Callback for changes to window state.  This includes
  // maximizing/restoring/minimizing the window.
  static gboolean OnWindowStateChanged(GtkWindow* window,
                                       GdkEventWindowState* event,
                                       BrowserTitlebar* titlebar);

  // Callback for mousewheel events.
  static gboolean OnScroll(GtkWidget* widget, GdkEventScroll* event,
                           BrowserTitlebar* titlebar);

  // Callback for min/max/close buttons.
  static void OnButtonClicked(GtkWidget* button, BrowserTitlebar* window);

  // Callback for favicon.
  static gboolean OnButtonPressed(GtkWidget* widget, GdkEventButton* event,
                                  BrowserTitlebar* titlebar);

  // -- Context Menu -----------------------------------------------------------

  // MenuGtk::Delegate implementation:
  virtual bool IsCommandEnabled(int command_id) const;
  virtual bool IsItemChecked(int command_id) const;
  virtual void ExecuteCommandById(int command_id);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overriden from ActiveWindowWatcher::Observer.
  virtual void ActiveWindowChanged(GdkWindow* active_window);

  // Pointers to the browser window that owns us and it's GtkWindow.
  BrowserWindowGtk* browser_window_;
  GtkWindow* window_;

  // The container widget the holds the whole titlebar.
  GtkWidget* container_;
  // Box that holds the min/max/close buttons if the user turns off window
  // manager decorations.
  GtkWidget* titlebar_buttons_box_;
  // Gtk alignment that contains the tab strip.  If the user turns off window
  // manager decorations, we draw this taller.
  GtkWidget* titlebar_alignment_;

  // Padding between the titlebar buttons and the top of the screen. Only show
  // when not maximized.
  GtkWidget* top_padding_;

  // The favicon and page title used when in app mode or popup mode.
  GtkWidget* app_mode_favicon_;
  GtkWidget* app_mode_title_;

  // Whether we are using a custom frame.
  bool using_custom_frame_;

  // Whether we have focus (gtk_window_is_active() sometimes returns the wrong
  // value, so manually track the focus-in and focus-out events.)
  bool window_has_focus_;

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

  // The context menu.
  scoped_ptr<MenuGtk> context_menu_;

  // The favicon menu.
  scoped_ptr<MenuGtk> favicon_menu_;

  // The throbber used when the window is in app mode or popup window mode.
  Throbber throbber_;

  // Theme provider for building buttons.
  GtkThemeProvider* theme_provider_;

  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_GTK_BROWSER_TITLEBAR_H_
