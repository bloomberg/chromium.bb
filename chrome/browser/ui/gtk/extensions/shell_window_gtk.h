// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_SHELL_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_SHELL_WINDOW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/x/active_window_watcher_x_observer.h"
#include "ui/gfx/rect.h"

class Profile;

class ShellWindowGtk : public ShellWindow,
                       public ExtensionViewGtk::Container,
                       public ui::ActiveWindowWatcherXObserver {
 public:
  ShellWindowGtk(Profile* profile,
                 const Extension* extension,
                 const GURL& url);

  // BaseWindow implementation.
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetDraggableRegion(SkRegion* region) OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;

  // ActiveWindowWatcherXObserver implementation.
  virtual void ActiveWindowChanged(GdkWindow* active_window) OVERRIDE;

 private:
  virtual ~ShellWindowGtk();

  CHROMEGTK_CALLBACK_1(ShellWindowGtk, gboolean, OnMainWindowDeleteEvent,
                       GdkEvent*);
  CHROMEGTK_CALLBACK_1(ShellWindowGtk, gboolean, OnConfigure,
                       GdkEventConfigure*);
  CHROMEGTK_CALLBACK_1(ShellWindowGtk, gboolean, OnWindowState,
                       GdkEventWindowState*);

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

  DISALLOW_COPY_AND_ASSIGN(ShellWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_SHELL_WINDOW_GTK_H_
