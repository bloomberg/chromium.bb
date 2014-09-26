// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/tab_scrubber.h"

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/views/controls/glow_hover_controller.h"

namespace {
const int64 kActivationDelayMS = 200;
}

// static
TabScrubber* TabScrubber::GetInstance() {
  static TabScrubber* instance = NULL;
  if (!instance)
    instance = new TabScrubber();
  return instance;
}

// static
gfx::Point TabScrubber::GetStartPoint(
    TabStrip* tab_strip,
    int index,
    TabScrubber::Direction direction) {
  int initial_tab_offset = Tab::GetMiniWidth() / 2;
  gfx::Rect tab_bounds = tab_strip->tab_at(index)->bounds();
  float x = direction == LEFT ?
      tab_bounds.x() + initial_tab_offset :
          tab_bounds.right() - initial_tab_offset;
  return gfx::Point(x, tab_bounds.CenterPoint().y());
}

bool TabScrubber::IsActivationPending() {
  return activate_timer_.IsRunning();
}

TabScrubber::TabScrubber()
    : scrubbing_(false),
      browser_(NULL),
      swipe_x_(-1),
      swipe_y_(-1),
      swipe_direction_(LEFT),
      highlighted_tab_(-1),
      activate_timer_(true, false),
      activation_delay_(kActivationDelayMS),
      use_default_activation_delay_(true),
      weak_ptr_factory_(this) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());
}

TabScrubber::~TabScrubber() {
  // Note: The weak_ptr_factory_ should invalidate  its weak pointers before
  // any other members are destroyed.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void TabScrubber::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL_FLING_CANCEL ||
      event->type() == ui::ET_SCROLL_FLING_START) {
    FinishScrub(true);
    immersive_reveal_lock_.reset();
    return;
  }

  if (event->finger_count() != 3)
    return;

  Browser* browser = GetActiveBrowser();
  if (!browser || (scrubbing_ && browser_ && browser != browser_) ||
      (highlighted_tab_ != -1 &&
          highlighted_tab_ >= browser->tab_strip_model()->count())) {
    FinishScrub(false);
    return;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(
          browser->window()->GetNativeWindow());
  TabStrip* tab_strip = browser_view->tabstrip();

  if (tab_strip->IsAnimating()) {
    FinishScrub(false);
    return;
  }

  // We are handling the event.
  event->StopPropagation();

  float x_offset = event->x_offset();
  int last_tab_index = highlighted_tab_ == -1 ?
      browser->tab_strip_model()->active_index() : highlighted_tab_;
  if (!scrubbing_) {
    swipe_direction_ = (x_offset < 0) ? LEFT : RIGHT;
    const gfx::Point start_point =
        GetStartPoint(tab_strip,
                      browser->tab_strip_model()->active_index(),
                      swipe_direction_);
    browser_ = browser;
    scrubbing_ = true;

    swipe_x_ = start_point.x();
    swipe_y_ = start_point.y();
    ImmersiveModeController* immersive_controller =
        browser_view->immersive_mode_controller();
    if (immersive_controller->IsEnabled()) {
      immersive_reveal_lock_.reset(immersive_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES));
    }
    tab_strip->AddObserver(this);
  } else if (highlighted_tab_ == -1) {
    Direction direction = (x_offset < 0) ? LEFT : RIGHT;
    if (direction != swipe_direction_) {
      const gfx::Point start_point =
          GetStartPoint(tab_strip,
                        browser->tab_strip_model()->active_index(),
                        direction);
      swipe_x_ = start_point.x();
      swipe_y_ = start_point.y();
      swipe_direction_ = direction;
    }
  }

  swipe_x_ += x_offset;
  Tab* first_tab = tab_strip->tab_at(0);
  int first_tab_center = first_tab->bounds().CenterPoint().x();
  Tab* last_tab = tab_strip->tab_at(tab_strip->tab_count() - 1);
  int last_tab_tab_center = last_tab->bounds().CenterPoint().x();
  if (swipe_x_ < first_tab_center)
    swipe_x_ = first_tab_center;
  if (swipe_x_ > last_tab_tab_center)
    swipe_x_ = last_tab_tab_center;

  Tab* initial_tab = tab_strip->tab_at(last_tab_index);
  gfx::Point tab_point(swipe_x_, swipe_y_);
  views::View::ConvertPointToTarget(tab_strip, initial_tab, &tab_point);
  Tab* new_tab = tab_strip->GetTabAt(initial_tab, tab_point);
  if (!new_tab)
    return;

  int new_index = tab_strip->GetModelIndexOfTab(new_tab);
  if (highlighted_tab_ == -1 &&
      new_index == browser->tab_strip_model()->active_index())
    return;

  if (new_index != highlighted_tab_) {
    if (activate_timer_.IsRunning()) {
      activate_timer_.Reset();
    } else {
      int delay = use_default_activation_delay_ ?
          ui::GestureConfiguration::tab_scrub_activation_delay_in_ms() :
          activation_delay_;
      if (delay >= 0) {
        activate_timer_.Start(FROM_HERE,
                              base::TimeDelta::FromMilliseconds(delay),
                              base::Bind(&TabScrubber::FinishScrub,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         true));
      }
    }
    if (highlighted_tab_ != -1) {
      Tab* tab = tab_strip->tab_at(highlighted_tab_);
      tab->hover_controller()->HideImmediately();
    }
    if (new_index == browser->tab_strip_model()->active_index()) {
      highlighted_tab_ = -1;
    } else {
      highlighted_tab_ = new_index;
      new_tab->hover_controller()->Show(views::GlowHoverController::PRONOUNCED);
    }
  }
  if (highlighted_tab_ != -1) {
    gfx::Point hover_point(swipe_x_, swipe_y_);
    views::View::ConvertPointToTarget(tab_strip, new_tab, &hover_point);
    new_tab->hover_controller()->SetLocation(hover_point);
  }
}

void TabScrubber::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  if (content::Source<Browser>(source).ptr() == browser_) {
    activate_timer_.Stop();
    swipe_x_ = -1;
    swipe_y_ = -1;
    scrubbing_ = false;
    highlighted_tab_ = -1;
    browser_ = NULL;
  }
}

void TabScrubber::TabStripAddedTabAt(TabStrip* tab_strip, int index) {
  if (highlighted_tab_ == -1)
    return;

  if (index < highlighted_tab_)
    ++highlighted_tab_;
}

void TabScrubber::TabStripMovedTab(TabStrip* tab_strip,
                                   int from_index,
                                   int to_index) {
  if (highlighted_tab_ == -1)
    return;

  if (from_index == highlighted_tab_)
    highlighted_tab_ = to_index;
  else if (from_index < highlighted_tab_&& highlighted_tab_<= to_index)
    --highlighted_tab_;
  else if (from_index > highlighted_tab_ && highlighted_tab_ >= to_index)
    ++highlighted_tab_;
}

void TabScrubber::TabStripRemovedTabAt(TabStrip* tab_strip, int index) {
  if (highlighted_tab_ == -1)
    return;
  if (index == highlighted_tab_) {
    FinishScrub(false);
    return;
  }
  if (index < highlighted_tab_)
    --highlighted_tab_;
}

void TabScrubber::TabStripDeleted(TabStrip* tab_strip) {
  if (highlighted_tab_ == -1)
    return;
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

void TabScrubber::FinishScrub(bool activate) {
  activate_timer_.Stop();

  if (browser_ && browser_->window()) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForNativeWindow(
            browser_->window()->GetNativeWindow());
    TabStrip* tab_strip = browser_view->tabstrip();
    if (activate && highlighted_tab_ != -1) {
      Tab* tab = tab_strip->tab_at(highlighted_tab_);
      tab->hover_controller()->HideImmediately();
      int distance =
          std::abs(
              highlighted_tab_ - browser_->tab_strip_model()->active_index());
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.ScrubDistance", distance, 0, 20, 21);
      browser_->tab_strip_model()->ActivateTabAt(highlighted_tab_, true);
    }
    tab_strip->RemoveObserver(this);
  }
  swipe_x_ = -1;
  swipe_y_ = -1;
  scrubbing_ = false;
  highlighted_tab_ = -1;
}
