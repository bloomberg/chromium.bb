// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/tab_scrubber.h"

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"

namespace {
Tab* GetTabAt(TabStrip* tab_strip, gfx::Point point) {
  for (int i = 0; i < tab_strip->tab_count(); ++i) {
    Tab* tab = tab_strip->tab_at(i);
    if (tab_strip->tab_at(i)->bounds().Contains(point))
      return tab;
  }
  return NULL;
}

const int kInitialTabOffset = 10;
}

// static
TabScrubber* TabScrubber::GetInstance() {
  static TabScrubber* instance = NULL;
  if (!instance)
    instance = new TabScrubber();
  return instance;
}

TabScrubber::TabScrubber()
    : scrubbing_(false),
      browser_(NULL),
      scroll_x_(-1),
      scroll_y_(-1) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

TabScrubber::~TabScrubber() {
}

void TabScrubber::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    if (scrubbing_)
      StopScrubbing();
    return;
  }

  if (event->finger_count() != 3 ||
      event->type() != ui::ET_SCROLL)
    return;

  Browser* browser = GetActiveBrowser();
  if (!browser || (browser_ && browser != browser_)) {
    if (scrubbing_)
      StopScrubbing();
    return;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(
          browser->window()->GetNativeWindow());
  TabStrip* tab_strip = browser_view->tabstrip();

  float x_offset = -event->x_offset();
  if (!scrubbing_) {
    scrubbing_ = true;
    browser_ = browser;
    Tab* initial_tab =
        tab_strip->tab_at(browser_->tab_strip_model()->active_index());
    scroll_x_ = initial_tab->x();
    scroll_x_ += (x_offset < 0) ?
        kInitialTabOffset : initial_tab->width() - kInitialTabOffset;
    scroll_y_ = initial_tab->height() / 2;
    registrar_.Add(
        this,
        chrome::NOTIFICATION_BROWSER_CLOSING,
        content::Source<Browser>(browser_));
  }

  if (ui::IsNaturalScrollEnabled())
    scroll_x_ += event->x_offset();
  else
    scroll_x_ -= event->x_offset();
  Tab* first_tab = tab_strip->tab_at(0);
  Tab* last_tab = tab_strip->tab_at(tab_strip->tab_count() - 1);
  if (scroll_x_ < first_tab->x())
    scroll_x_ = first_tab->x();
  if (scroll_x_ > last_tab->bounds().right())
    scroll_x_ = last_tab->bounds().right();

  gfx::Point tab_point(scroll_x_, scroll_y_);
  Tab* new_tab = GetTabAt(tab_strip, tab_point);
  if (new_tab && !new_tab->IsActive()) {
    int new_index = tab_strip->GetModelIndexOfTab(new_tab);
    browser->tab_strip_model()->ActivateTabAt(new_index, true);
  }

  event->StopPropagation();
}

void TabScrubber::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_CLOSING &&
         content::Source<Browser>(source).ptr() == browser_);
  StopScrubbing();
}

Browser* TabScrubber::GetActiveBrowser() {
  aura::Window* active_window = ash::wm::GetActiveWindow();
  if (!active_window)
    return NULL;

  Browser* browser = chrome::FindBrowserWithWindow(active_window);
  if (!browser || browser->type() != Browser::TYPE_TABBED)
    return NULL;

  return browser;
}

void TabScrubber::StopScrubbing() {
  if (!scrubbing_)
    return;

  registrar_.Remove(
      this,
      chrome::NOTIFICATION_BROWSER_CLOSING,
      content::Source<Browser>(browser_));
  scrubbing_ = false;
  browser_ = NULL;
}
