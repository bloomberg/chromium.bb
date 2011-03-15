// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/tabs/touch_tab_strip.h"

#include "chrome/browser/ui/touch/tabs/touch_tab.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "ui/gfx/canvas_skia.h"
#include "views/metrics.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

static const int kTouchTabStripHeight = 64;
static const int kTouchTabWidth = 64;
static const int kTouchTabHeight = 64;

TouchTabStrip::TouchTabStrip(TabStripController* controller)
    : BaseTabStrip(controller, BaseTabStrip::HORIZONTAL_TAB_STRIP),
      in_tab_close_(false),
      last_tap_time_(base::Time::FromInternalValue(0)),
      last_tapped_view_(NULL) {
  Init();
}

TouchTabStrip::~TouchTabStrip() {
  // The animations may reference the tabs. Shut down the animation before we
  // delete the tabs.
  StopAnimating(false);

  DestroyDragController();

  // The children (tabs) may callback to us from their destructor. Delete them
  // so that if they call back we aren't in a weird state.
  RemoveAllChildViews(true);
}

////////////////////////////////////////////////////////////////////////////////
// TouchTabStrip, AbstractTabStripView implementation:

bool TouchTabStrip::IsPositionInWindowCaption(const gfx::Point& point) {
  // The entire tabstrip is mine. No part of it belongs to the window caption.
  return false;
}

void TouchTabStrip::SetBackgroundOffset(const gfx::Point& offset) {
  for (int i = 0; i < tab_count(); ++i)
    GetTabAtTabDataIndex(i)->set_background_offset(offset);
}

////////////////////////////////////////////////////////////////////////////////
// TouchTabStrip, BaseTabStrip implementation:

void TouchTabStrip::PrepareForCloseAt(int model_index) {
  if (!in_tab_close_ && IsAnimating()) {
    // Cancel any current animations. We do this as remove uses the current
    // ideal bounds and we need to know ideal bounds is in a good state.
    StopAnimating(true);
  }

  in_tab_close_ = true;
}

void TouchTabStrip::StartHighlight(int model_index) {
}

void TouchTabStrip::StopAllHighlighting() {
}

BaseTab* TouchTabStrip::CreateTabForDragging() {
  return NULL;
}

void TouchTabStrip::RemoveTabAt(int model_index) {
  StartRemoveTabAnimation(model_index);
}

void TouchTabStrip::SelectTabAt(int old_model_index, int new_model_index) {
  SchedulePaint();
}

void TouchTabStrip::TabTitleChangedNotLoading(int model_index) {
}

BaseTab* TouchTabStrip::CreateTab() {
  return new TouchTab(this);
}

void TouchTabStrip::StartInsertTabAnimation(int model_index) {
  PrepareForAnimation();

  in_tab_close_ = false;

  GenerateIdealBounds();

  int tab_data_index = ModelIndexToTabIndex(model_index);
  BaseTab* tab = base_tab_at_tab_index(tab_data_index);
  if (model_index == 0) {
    tab->SetBounds(0, ideal_bounds(tab_data_index).y(), 0,
                   ideal_bounds(tab_data_index).height());
  } else {
    BaseTab* last_tab = base_tab_at_tab_index(tab_data_index - 1);
    tab->SetBounds(last_tab->bounds().right(),
                   ideal_bounds(tab_data_index).y(), 0,
                   ideal_bounds(tab_data_index).height());
  }

  AnimateToIdealBounds();
}

void TouchTabStrip::AnimateToIdealBounds() {
  for (int i = 0; i < tab_count(); ++i) {
    TouchTab* tab = GetTabAtTabDataIndex(i);
    if (!tab->closing() && !tab->dragging())
      bounds_animator().AnimateViewTo(tab, ideal_bounds(i));
  }
}

bool TouchTabStrip::ShouldHighlightCloseButtonAfterRemove() {
  return in_tab_close_;
}

void TouchTabStrip::GenerateIdealBounds() {
  gfx::Rect bounds;
  int tab_x = 0;
  int tab_y = 0;
  for (int i = 0; i < tab_count(); ++i) {
    TouchTab* tab = GetTabAtTabDataIndex(i);
    if (!tab->closing()) {
      set_ideal_bounds(i, gfx::Rect(tab_x, tab_y, kTouchTabWidth,
                                    kTouchTabHeight));
      // offset the next tab to the right by the width of this tab
      tab_x += kTouchTabWidth;
    }
  }
}

void TouchTabStrip::LayoutDraggedTabsAt(const std::vector<BaseTab*>& tabs,
                                        const gfx::Point& location) {
  // Not needed as dragging isn't supported.
  NOTIMPLEMENTED();
}

int TouchTabStrip::GetSizeNeededForTabs(const std::vector<BaseTab*>& tabs) {
  // Not needed as dragging isn't supported.
  NOTIMPLEMENTED();
  return 0;
}

TouchTab* TouchTabStrip::GetTabAtTabDataIndex(int tab_data_index) const {
  return static_cast<TouchTab*>(base_tab_at_tab_index(tab_data_index));
}

////////////////////////////////////////////////////////////////////////////////
// TouchTabStrip, private:

void TouchTabStrip::Init() {
  SetID(VIEW_ID_TAB_STRIP);
}

////////////////////////////////////////////////////////////////////////////////
// TouchTabStrip, views::View overrides, private:

gfx::Size TouchTabStrip::GetPreferredSize() {
  return gfx::Size(0, kTouchTabStripHeight);
}

void TouchTabStrip::PaintChildren(gfx::Canvas* canvas) {
  // Tabs are painted in reverse order, so they stack to the left.
  TouchTab* selected_tab = NULL;
  TouchTab* dragging_tab = NULL;

  for (int i = tab_count() - 1; i >= 0; --i) {
    TouchTab* tab = GetTabAtTabDataIndex(i);
    // We must ask the _Tab's_ model, not ourselves, because in some situations
    // the model will be different to this object, e.g. when a Tab is being
    // removed after its TabContents has been destroyed.
    if (tab->dragging()) {
      dragging_tab = tab;
    } else if (!tab->IsSelected()) {
      tab->Paint(canvas);
    } else {
      selected_tab = tab;
    }
  }

  if (GetWindow()->non_client_view()->UseNativeFrame()) {
    // Make sure unselected tabs are somewhat transparent.
    SkPaint paint;
    paint.setColor(SkColorSetARGB(200, 255, 255, 255));
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->DrawRectInt(0, 0, width(),
        height() - 2,  // Visible region that overlaps the toolbar.
        paint);
  }

  // Paint the selected tab last, so it overlaps all the others.
  if (selected_tab)
    selected_tab->Paint(canvas);

  // And the dragged tab.
  if (dragging_tab)
    dragging_tab->Paint(canvas);
}

views::View::TouchStatus TouchTabStrip::OnTouchEvent(
    const views::TouchEvent& event) {
  if (event.type() != ui::ET_TOUCH_PRESSED)
    return TOUCH_STATUS_UNKNOWN;

  views::View* view = GetEventHandlerForPoint(event.location());
  if (view && view != this && view->GetID() != VIEW_ID_TAB)
    return TOUCH_STATUS_UNKNOWN;

  base::TimeDelta delta = event.time_stamp() - last_tap_time_;

  if (delta.InMilliseconds() < views::GetDoubleClickInterval() &&
      view == last_tapped_view_) {
    // If double tapped the empty space, open a new tab. If double tapped a tab,
    // close it.
    if (view == this)
      controller()->CreateNewTab();
    else
      CloseTab(static_cast<BaseTab*>(view));

    last_tap_time_ = base::Time::FromInternalValue(0);
    last_tapped_view_ = NULL;
    return TOUCH_STATUS_END;
  }

  last_tap_time_ = event.time_stamp();
  last_tapped_view_ = view;
  return TOUCH_STATUS_UNKNOWN;
}

void TouchTabStrip::ViewHierarchyChanged(bool is_add,
                                         View* parent,
                                         View* child) {
  if (!is_add && last_tapped_view_ == child)
    last_tapped_view_ = NULL;
}
