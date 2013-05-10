// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_PANELS_PANEL_STACK_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_PANELS_PANEL_STACK_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include <list>
#include <map>

#include "chrome/browser/ui/panels/native_panel_stack_window.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/x/active_window_watcher_x_observer.h"

class PanelStackWindowGtk : public NativePanelStackWindow,
                            public ui::ActiveWindowWatcherXObserver {
 public:
  explicit PanelStackWindowGtk(NativePanelStackWindowDelegate* delegate);
  virtual ~PanelStackWindowGtk();

 protected:
  // Overridden from NativePanelStackWindow:
  virtual void Close() OVERRIDE;
  virtual void AddPanel(Panel* panel) OVERRIDE;
  virtual void RemovePanel(Panel* panel) OVERRIDE;
  virtual void MergeWith(NativePanelStackWindow* another) OVERRIDE;
  virtual bool IsEmpty() const OVERRIDE;
  virtual bool HasPanel(Panel* panel) const OVERRIDE;
  virtual void MovePanelsBy(const gfx::Vector2d& delta) OVERRIDE;
  virtual void BeginBatchUpdatePanelBounds(bool animate) OVERRIDE;
  virtual void AddPanelBoundsForBatchUpdate(Panel* panel,
                                            const gfx::Rect& bounds) OVERRIDE;
  virtual void EndBatchUpdatePanelBounds() OVERRIDE;
  virtual bool IsAnimatingPanelBounds() const OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void DrawSystemAttention(bool draw_attention) OVERRIDE;
  virtual void OnPanelActivated(Panel* panel) OVERRIDE;

 private:
  typedef std::list<Panel*> Panels;

  // Overridden from ActiveWindowWatcherXObserver.
  virtual void ActiveWindowChanged(GdkWindow* active_window) OVERRIDE;

  CHROMEGTK_CALLBACK_1(PanelStackWindowGtk, gboolean, OnWindowDeleteEvent,
                       GdkEvent*);
  CHROMEGTK_CALLBACK_1(PanelStackWindowGtk, gboolean, OnWindowState,
                       GdkEventWindowState*);

  void EnsureWindowCreated();
  void SetStackWindowBounds();

  // The map value is new bounds of the panel.
  typedef std::map<Panel*, gfx::Rect> BoundsUpdates;

  NativePanelStackWindowDelegate* delegate_;

  // The background window that provides the aggregated taskbar presence for all
  // the panels in the stack.
  GtkWindow* window_;

  Panels panels_;

  bool is_minimized_;

  // For batch bounds update.
  bool bounds_updates_started_;
  BoundsUpdates bounds_updates_;

  DISALLOW_COPY_AND_ASSIGN(PanelStackWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_PANELS_PANEL_STACK_WINDOW_GTK_H_

