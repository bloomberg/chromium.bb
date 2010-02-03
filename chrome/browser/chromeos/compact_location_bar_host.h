// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_HOST_H_
#define CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_HOST_H_

#include "app/animation.h"
#include "app/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "base/timer.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/dropdown_bar_host.h"
#include "views/controls/textfield/textfield.h"

class BrowserView;
class TabContents;
class Tab;

namespace chromeos {

class CompactLocationBarView;
class MouseObserver;

////////////////////////////////////////////////////////////////////////////////
//
// The CompactLocationBarHost implements the container window for the
// floating location bar. It uses the appropriate implementation from
// compact_location_bar_host_gtk.cc to draw its content and is
// responsible for showing, hiding, closing, and moving the window.
//
// There is one CompactLocationBarHost per BrowserView, and its state
// is updated whenever the selected Tab is changed. The
// CompactLocationBarHost is created when the BrowserView is attached
// to the frame's Widget for the first time, and enabled/disabled
// when the compact navigation bar is toggled.
//
////////////////////////////////////////////////////////////////////////////////
class CompactLocationBarHost : public DropdownBarHost,
                               public TabStripModelObserver {
 public:
  explicit CompactLocationBarHost(BrowserView* browser_view);
  virtual ~CompactLocationBarHost();

  // Returns the bounds to locale the compact location bar under the tab.
  gfx::Rect GetBoundsUnderTab(int tab_index) const;

  // Updates the content and the position of the compact location bar.
  // |index| is the index of the tab the compact location bar
  // will be attached to and |animate| specifies if the location bar
  // should animate when shown.
  void Update(int index, bool animate);

  // (Re)Starts the popup timer that hides the popup after X seconds.
  void StartAutoHideTimer();

  // Cancels the popup timer.
  void CancelAutoHideTimer();

  // Enable/disable the compact location bar.
  void SetEnabled(bool enabled);

  // Overridehden from DropdownBarhost.
  virtual void Show(bool animate);
  virtual void Hide(bool animate);

  // Overridden from views::AcceleratorTarget in DropdownBarHost class.
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // Overridden from DropdownBarHost class.
  virtual gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);
  virtual void SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw);

  // Overriden from TabStripModelObserver class.
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabClosingAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index);
  virtual void TabChangedAt(TabContents* contents, int index,
                            TabChangeType change_type);

 private:
  friend class MouseObserver;

  void HideCallback() {
    Hide(true);
  }

  // Returns CompactLocationBarView.
  CompactLocationBarView* GetClbView();

  bool IsCurrentTabIndex(int index);

  // The index of the tab that the compact location bar is attached to.
  int current_tab_index_;

  scoped_ptr<base::OneShotTimer<CompactLocationBarHost> > auto_hide_timer_;

  scoped_ptr<MouseObserver> mouse_observer_;

  DISALLOW_COPY_AND_ASSIGN(CompactLocationBarHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_HOST_H_
