// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_collection_impl.h"

#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/views/notifications/balloon_view.h"
#include "gfx/rect.h"

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
  const Balloons& balloons = base_.balloons();
  if (balloons.empty())
    return false;

  gfx::Point upper_left = balloons[balloons.size() - 1]->GetPosition();
  gfx::Point lower_right = layout_.GetLayoutOrigin();

  gfx::Rect bounds = gfx::Rect(upper_left.x(),
                               upper_left.y(),
                               lower_right.x() - upper_left.x(),
                               lower_right.y() - upper_left.y());

  DWORD pos = GetMessagePos();
  gfx::Point cursor(pos);

  return bounds.Contains(cursor);
}

// static
BalloonCollection* BalloonCollection::Create() {
  return new BalloonCollectionImpl();
}
