// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_collection_impl.h"

#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/ui/views/notifications/balloon_view_views.h"
#include "ui/base/events/event_constants.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

Balloon* BalloonCollectionImpl::MakeBalloon(const Notification& notification,
                                            Profile* profile) {
  Balloon* balloon = new Balloon(notification, profile, this);
  balloon->set_view(new BalloonViewImpl(this));
  gfx::Size size(layout_.min_balloon_width(), layout_.min_balloon_height());
  balloon->set_content_size(size);
  return balloon;
}

int BalloonCollectionImpl::Layout::InterBalloonMargin() const {
  return 3;
}

int BalloonCollectionImpl::Layout::HorizontalEdgeMargin() const {
  return 2;
}

int BalloonCollectionImpl::Layout::VerticalEdgeMargin() const {
  return 0;
}

bool BalloonCollectionImpl::Layout::NeedToMoveAboveLeftSidePanels() const {
  return placement_ == VERTICALLY_FROM_BOTTOM_LEFT;
}

bool BalloonCollectionImpl::Layout::NeedToMoveAboveRightSidePanels() const {
  return placement_ == VERTICALLY_FROM_BOTTOM_RIGHT;
}

void BalloonCollectionImpl::PositionBalloons(bool reposition) {
  PositionBalloonsInternal(reposition);
}

base::EventStatus BalloonCollectionImpl::WillProcessEvent(
    const base::NativeEvent& event) {
  return base::EVENT_CONTINUE;
}

void BalloonCollectionImpl::DidProcessEvent(const base::NativeEvent& event) {
#if defined(OS_WIN)
  switch (event.message) {
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      HandleMouseMoveEvent();
      break;
  }
#elif defined(USE_AURA)
  // This is deliberately used only in linux. For an aura build on windows, the
  // above block of code is fine.
  switch (ui::EventTypeFromNative(event)) {
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_EXITED:
      HandleMouseMoveEvent();
      break;
    default:
      break;
  }
#else
  NOTIMPLEMENTED();
#endif
}

bool BalloonCollectionImpl::IsCursorInBalloonCollection() const {
#if defined(OS_WIN)
  gfx::Point cursor(GetMessagePos());
#else
  // TODO(saintlou): Not sure if this is correct because on Windows at least
  // the following call is GetCursorPos() not GetMessagePos().
  gfx::Point cursor(gfx::Screen::GetCursorScreenPoint());
#endif
  return GetBalloonsBoundingBox().Contains(cursor);
}

void BalloonCollectionImpl::SetPositionPreference(
    PositionPreference position) {
  if (position == DEFAULT_POSITION)
    position = LOWER_RIGHT;

  // All positioning schemes are vertical, and windows
  // uses the normal screen orientation.
  if (position == UPPER_RIGHT)
    layout_.set_placement(Layout::VERTICALLY_FROM_TOP_RIGHT);
  else if (position == UPPER_LEFT)
    layout_.set_placement(Layout::VERTICALLY_FROM_TOP_LEFT);
  else if (position == LOWER_LEFT)
    layout_.set_placement(Layout::VERTICALLY_FROM_BOTTOM_LEFT);
  else if (position == LOWER_RIGHT)
    layout_.set_placement(Layout::VERTICALLY_FROM_BOTTOM_RIGHT);
  else
    NOTREACHED();

  layout_.ComputeOffsetToMoveAbovePanels();
  PositionBalloons(true);
}

#if !(defined(USE_ASH) && defined(OS_CHROMEOS))
// static
BalloonCollection* BalloonCollection::Create() {
  return new BalloonCollectionImpl();
}
#endif
