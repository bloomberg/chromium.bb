// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_APPS_NATIVE_APP_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_APPS_NATIVE_APP_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include "apps/native_app_window.h"
#include "apps/shell_window.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/x/active_window_watcher_x_observer.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"

class ExtensionKeybindingRegistryGtk;
class Profile;

namespace extensions {
class Extension;
}

class NativeAppWindowGtk : public apps::NativeAppWindow,
                           public ExtensionViewGtk::Container,
                           public ui::ActiveWindowWatcherXObserver,
                           public content::WebContentsObserver {
 public:
  NativeAppWindowGtk(apps::ShellWindow* shell_window,
                     const apps::ShellWindow::CreateParams& params);

  // ui::BaseWindow implementation.
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual bool IsDetached() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual ui::WindowShowState GetRestoredState() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;

  // ActiveWindowWatcherXObserver implementation.
  virtual void ActiveWindowChanged(GdkWindow* active_window) OVERRIDE;

  // WebContentsObserver implementation.
  virtual void RenderViewHostChanged(
      content::RenderViewHost* old_host,
      content::RenderViewHost* new_host) OVERRIDE;

 private:
  // NativeAppWindow implementation.
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreenOrPending() const OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void UpdateInputRegion(scoped_ptr<SkRegion> region) OVERRIDE;
  virtual void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) OVERRIDE;
  virtual SkRegion* GetDraggableRegion() OVERRIDE;
  virtual bool IsFrameless() const OVERRIDE;
  virtual gfx::Insets GetFrameInsets() const OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void HideWithApp() OVERRIDE;
  virtual void ShowWithApp() OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;

  // web_modal::WebContentsModalDialogHost implementation.
  virtual gfx::NativeView GetHostView() const OVERRIDE;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE;
  virtual gfx::Size GetMaximumDialogSize() OVERRIDE;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;

  content::WebContents* web_contents() const {
    return shell_window_->web_contents();
  }
  const extensions::Extension* extension() const {
    return shell_window_->extension();
  }

  virtual ~NativeAppWindowGtk();

  // If the point (|x|, |y|) is within the resize border area of the window,
  // returns true and sets |edge| to the appropriate GdkWindowEdge value.
  // Otherwise, returns false.
  bool GetWindowEdge(int x, int y, GdkWindowEdge* edge);

  CHROMEGTK_CALLBACK_1(NativeAppWindowGtk, gboolean, OnMainWindowDeleteEvent,
                       GdkEvent*);
  CHROMEGTK_CALLBACK_1(NativeAppWindowGtk, gboolean, OnConfigure,
                       GdkEventConfigure*);
  CHROMEGTK_CALLBACK_1(NativeAppWindowGtk, gboolean, OnWindowState,
                       GdkEventWindowState*);
  CHROMEGTK_CALLBACK_1(NativeAppWindowGtk, gboolean, OnMouseMoveEvent,
                       GdkEventMotion*);
  CHROMEGTK_CALLBACK_1(NativeAppWindowGtk, gboolean, OnButtonPress,
                       GdkEventButton*);
  // Callback for PropertyChange XEvents.
  CHROMEG_CALLBACK_1(NativeAppWindowGtk, GdkFilterReturn,
                     OnXEvent, GdkXEvent*, GdkEvent*);

  void OnConfigureDebounced();

  apps::ShellWindow* shell_window_;  // weak - ShellWindow owns NativeAppWindow.

  GtkWindow* window_;
  GdkWindowState state_;

  // True if the window manager thinks the window is active.  Not all window
  // managers keep track of this state (_NET_ACTIVE_WINDOW), in which case
  // this will always be true.
  bool is_active_;

  // The position and size of the current window.
  gfx::Rect bounds_;

  // The position and size of the non-maximized, non-fullscreen window.
  gfx::Rect restored_bounds_;

  // True if the RVH is in fullscreen mode. The window may not actually be in
  // fullscreen, however: some WMs don't support fullscreen.
  bool content_thinks_its_fullscreen_;

  // The region is treated as title bar, can be dragged to move
  // and double clicked to maximize.
  scoped_ptr<SkRegion> draggable_region_;

  // If true, don't call gdk_window_raise() when we get a click in the title
  // bar or window border.  This is to work around a compiz bug.
  bool suppress_window_raise_;

  // True if the window shows without frame.
  bool frameless_;

  // True if the window should be resizable by the user.
  bool resizable_;

  // True if the window should be kept on top of other windows that do not have
  // this flag enabled.
  bool always_on_top_;

  // The current window cursor.  We set it to a resize cursor when over the
  // custom frame border.  We set it to NULL if we want the default cursor.
  GdkCursor* frame_cursor_;

  // The timer used to save the window position for session restore.
  base::OneShotTimer<NativeAppWindowGtk> window_configure_debounce_timer_;

  // The Extension Keybinding Registry responsible for registering listeners for
  // accelerators that are sent to the window, that are destined to be turned
  // into events and sent to the extension.
  scoped_ptr<ExtensionKeybindingRegistryGtk> extension_keybinding_registry_;

  // Observers to be notified when any web contents modal dialog requires
  // updating its dimensions.
  ObserverList<web_modal::ModalDialogHostObserver> observer_list_;

  ui::X11AtomCache atom_cache_;

  // True if we listen for the XEvent.
  bool is_x_event_listened_;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_APPS_NATIVE_APP_WINDOW_GTK_H_
