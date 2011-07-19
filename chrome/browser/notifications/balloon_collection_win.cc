// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_collection_impl.h"

#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/ui/views/notifications/balloon_view.h"
#include "ui/gfx/rect.h"

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

void BalloonCollectionImpl::PositionBalloons(bool reposition) {
  PositionBalloonsInternal(reposition);
}

void BalloonCollectionImpl::DidProcessMessage(const MSG& msg) {
  switch (msg.message) {
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      HandleMouseMoveEvent();
      break;
  }
}

bool BalloonCollectionImpl::IsCursorInBalloonCollection() const {
  DWORD pos = GetMessagePos();
  gfx::Point cursor(pos);
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

  PositionBalloons(true);
}

// static
BalloonCollection* BalloonCollection::Create() {
  return new BalloonCollectionImpl();
}
