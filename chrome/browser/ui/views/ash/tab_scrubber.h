// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_TAB_SCRUBBER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_TAB_SCRUBBER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/events/event_handler.h"

class Browser;
class Tab;

// Class to enable quick tab switching via Ctrl-left-drag.
// Notes: this is experimental, and disables ctrl-clicks. It should not be
// enabled other than through flags until we implement 3 finger drag as the
// mechanism to invoke it. At that point we will add test coverage.
class TabScrubber : public ui::EventHandler,
                    public content::NotificationObserver {
 public:
  static TabScrubber* GetInstance();

 private:
  TabScrubber();
  virtual ~TabScrubber();

  // ui::EventHandler overrides:
  virtual ui::EventResult OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Browser* GetActiveBrowser();
  void StartScrubbing();
  void StopScrubbing();

  // Indicates that we are currently scrubbing.
  bool scrubbing_;
  // The browser that we are scrubbing.
  Browser* browser_;
  // The x value of the event that initiated scrubbing.
  float scroll_x_;
  float scroll_y_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabScrubber);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_TAB_SCRUBBER_H_
