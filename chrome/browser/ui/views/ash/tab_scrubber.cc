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
      initial_tab_index_(-1),
      initial_x_(-1) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

TabScrubber::~TabScrubber() {
}

ui::EventResult TabScrubber::OnMouseEvent(ui::MouseEvent* event) {
  Browser* browser = GetActiveBrowser();

  if (!(event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_DRAGGED ||
      event->type() == ui::ET_MOUSE_RELEASED))
    return ui::ER_UNHANDLED;

  if (!browser ||
      (event->type() == ui::ET_MOUSE_RELEASED) ||
      !(event->flags() & ui::EF_CONTROL_DOWN) ||
      !(event->flags() & ui::EF_LEFT_MOUSE_BUTTON) ||
      (browser_ && browser != browser_)) {
    if (scrubbing_)
      StopScrubbing();
    return ui::ER_UNHANDLED;
  }

  if (!scrubbing_) {
    scrubbing_ = true;
    initial_x_ = event->x();
    browser_ = browser;
    initial_tab_index_ = browser_->active_index();
    registrar_.Add(
        this,
        chrome::NOTIFICATION_BROWSER_CLOSING,
        content::Source<Browser>(browser_));
  } else {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForNativeWindow(
            browser_->window()->GetNativeWindow());
    TabStrip* tab_strip = browser_view->tabstrip();
    Tab* initial_tab = tab_strip->tab_at(initial_tab_index_);
    if (!initial_tab) {
      StopScrubbing();
      return ui::ER_UNHANDLED;
    }

    gfx::Point tab_point((initial_tab->width() / 2) + event->x() - initial_x_,
                         initial_tab->height() / 2);
    Tab* new_tab = tab_strip->GetTabAt(initial_tab, tab_point);
    if (new_tab && !new_tab->IsActive()) {
      int new_index = tab_strip->GetModelIndexOfTab(new_tab);
      browser->tab_strip_model()->ActivateTabAt(new_index, true);
    }
  }

  return ui::ER_CONSUMED;
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

  Browser* browser = browser::FindBrowserWithWindow(active_window);
  if (!browser || browser->type() != Browser::TYPE_TABBED)
    return NULL;

  return browser;
}

void TabScrubber::StopScrubbing() {
  registrar_.Remove(
      this,
      chrome::NOTIFICATION_BROWSER_CLOSING,
      content::Source<Browser>(browser_));
  scrubbing_ = false;
  browser_ = NULL;
  initial_tab_index_ = -1;
}
