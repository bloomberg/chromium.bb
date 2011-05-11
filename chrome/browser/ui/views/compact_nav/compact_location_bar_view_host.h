// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_LOCATION_BAR_VIEW_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_LOCATION_BAR_VIEW_HOST_H_

#include "base/timer.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

class BrowserView;
class CompactLocationBarView;
class DropdownBarHost;
class MouseObserver;
class NotificationObserver;
class NotificationRegistrar;
class TabContents;

namespace gfx {
class Rect;
}

////////////////////////////////////////////////////////////////////////////////
//
// The CompactLocationBarViewHost implements the container window for the
// floating location bar.
//
// There is one CompactLocationBarViewHost per BrowserView, and its state
// is updated whenever the selected Tab is changed. The
// CompactLocationBarViewHost is created when the BrowserView is attached
// to the frame's Widget for the first time, and enabled/disabled
// when the compact navigation bar is toggled.
//
////////////////////////////////////////////////////////////////////////////////
class CompactLocationBarViewHost : public DropdownBarHost,
                                   public TabStripModelObserver,
                                   public NotificationObserver,
                                   public LocationBarView::Delegate {
 public:
  explicit CompactLocationBarViewHost(BrowserView* browser_view);
  virtual ~CompactLocationBarViewHost();

  // Returns the bounds to locate the compact location bar under the tab. The
  // coordinate system is the browser frame for Windows, otherwise the browser
  // view.
  gfx::Rect GetBoundsUnderTab(int model_index) const;

  // Updates the content and the position of the compact location bar.
  // |model_index| is the index of the tab the compact location bar
  // will be attached to and |animate| specifies if the location bar
  // should animate when shown. The second version gets the actual |contents|
  // instead of the |model_index|.
  void UpdateOnTabChange(int model_index, bool animate);
  void Update(TabContents* contents, bool animate);

  // (Re)Starts the popup timer that hides the popup after X seconds.
  void StartAutoHideTimer();

  // Cancels the popup timer.
  void CancelAutoHideTimer();

  // Enable/disable the compact location bar.
  void SetEnabled(bool enabled);

  CompactLocationBarView* GetCompactLocationBarView();

  // Readjust the position of the host window while avoiding |selection_rect|.
  // |selection_rect| is expected to have coordinates relative to the top of
  // the web page area. If |no_redraw| is true, the window will be moved without
  // redrawing siblings.
  virtual void MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                     bool no_redraw);

  // Overridden from DropdownBarhost.
  virtual void Show(bool animate) OVERRIDE;
  virtual void Hide(bool animate) OVERRIDE;
  virtual gfx::Rect GetDialogPosition(
      gfx::Rect avoid_overlapping_rect) OVERRIDE;
  virtual void SetDialogPosition(const gfx::Rect& new_pos,
                                 bool no_redraw) OVERRIDE;

  // Overridden from views::AcceleratorTarget in DropdownBarHost class.
  virtual bool AcceleratorPressed(
      const views::Accelerator& accelerator) OVERRIDE;

  // Overridden from TabStripModelObserver class.
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index) OVERRIDE;
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture) OVERRIDE;
  virtual void TabMoved(TabContentsWrapper* contents,
                        int from_index,
                        int to_index) OVERRIDE;
  virtual void TabChangedAt(TabContentsWrapper* contents, int index,
                            TabChangeType change_type) OVERRIDE;
  virtual void ActiveTabClicked(int index) OVERRIDE;

  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // LocationBarView::Delegate overrides
  virtual TabContentsWrapper* GetTabContentsWrapper() const OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual void OnInputInProgress(bool in_progress) OVERRIDE;

 private:
  friend class MouseObserver;
  friend class CompactLocationBarViewHostTest;

  bool HasFocus();
  void HideCallback();
  bool IsCurrentTabIndex(int index);
  bool IsCurrentTab(TabContents* contents);

  // We use this to be notified of the end of a page load so we can hide.
  NotificationRegistrar registrar_;

  // The index of the tab, in terms of the model, that the compact location bar
  // is attached to.
  int current_tab_model_index_;

  scoped_ptr<base::OneShotTimer<CompactLocationBarViewHost> > auto_hide_timer_;

  scoped_ptr<MouseObserver> mouse_observer_;

  // Track if we are currently observing the tabstrip model.
  bool is_observing_;

  DISALLOW_COPY_AND_ASSIGN(CompactLocationBarViewHost);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_LOCATION_BAR_VIEW_HOST_H_
