// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_TAB_SCRUBBER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_TAB_SCRUBBER_H_

#include "base/timer.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/events/event_handler.h"

class Browser;
class TabStrip;

namespace gfx {
class Point;
}

// Class to enable quick tab switching via Ctrl-left-drag.
// Notes: this is experimental, and disables ctrl-clicks. It should not be
// enabled other than through flags until we implement 3 finger drag as the
// mechanism to invoke it. At that point we will add test coverage.
class TabScrubber : public ui::EventHandler,
                    public content::NotificationObserver,
                    public TabStripObserver {
 public:
  enum Direction {LEFT, RIGHT};

  // Returns a the single instance of a TabScrubber.
  static TabScrubber* GetInstance();

  // Returns the virtual position of a swipe starting in the tab at |index|,
  // base on the |direction|.
  static gfx::Point GetStartPoint(TabStrip* tab_strip,
                                  int index,
                                  TabScrubber::Direction direction);

  void set_activation_delay(base::TimeDelta activation_delay) {
    activation_delay_ = activation_delay;
  }
  base::TimeDelta activation_delay() const { return activation_delay_; }
  int highlighted_tab() const { return highlighted_tab_; }
  bool IsActivationPending();

 private:
  TabScrubber();
  virtual ~TabScrubber();

  // ui::EventHandler overrides:
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // TabStripObserver overrides.
  virtual void TabStripAddedTabAt(TabStrip* tab_strip, int index) OVERRIDE;
  virtual void TabStripMovedTab(TabStrip* tab_strip,
                                int from_index,
                                int to_index) OVERRIDE;
  virtual void TabStripRemovedTabAt(TabStrip* tab_strip, int index) OVERRIDE;
  virtual void TabStripDeleted(TabStrip* tab_strip) OVERRIDE;

  Browser* GetActiveBrowser();
  void FinishScrub(bool activate);
  void CancelImmersiveReveal();

  // Are we currently scrubbing?.
  bool scrubbing_;
  // The last browser we used for scrubbing, NULL if |scrubbing_| is
  // false and there is no pending work.
  Browser* browser_;
  // The current accumulated x and y positions of a swipe, in
  // the coordinates of the TabStrip of |browser_|
  float swipe_x_;
  float swipe_y_;
  // The direction the current swipe is headed.
  Direction swipe_direction_;
  // The index of the tab that is currently highlighted.
  int highlighted_tab_;
  // Timer to control a delayed activation of the |highlighted_tab_|.
  base::Timer activate_timer_;
  // Time to wait before newly selected tab becomes active.
  base::TimeDelta activation_delay_;
  // Indicates if we were in immersive mode and forced the tabs to be
  // revealed.
  bool should_cancel_immersive_reveal_;

  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<TabScrubber> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabScrubber);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_TAB_SCRUBBER_H_
