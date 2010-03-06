// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"

#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/chromeos/notifications/balloon_view.h"
#include "chrome/browser/chromeos/notifications/notification_panel.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/window_sizer.h"

namespace {

// Portion of the screen allotted for notifications. When notification balloons
// extend over this, no new notifications are shown until some are closed.
const double kPercentBalloonFillFactor = 0.7;

// Allow at least this number of balloons on the screen.
const int kMinAllowedBalloonCount = 2;

// Margin from the edge of the work area
const int kVerticalEdgeMargin = 5;
const int kHorizontalEdgeMargin = 5;

}  // namespace

namespace chromeos {

BalloonCollectionImpl::BalloonCollectionImpl()
    : panel_(new NotificationPanel()) {
}

BalloonCollectionImpl::~BalloonCollectionImpl() {
  STLDeleteElements(&balloons_);
}

void BalloonCollectionImpl::Add(const Notification& notification,
                                Profile* profile) {
  Balloon* new_balloon = MakeBalloon(notification, profile);
  balloons_.push_back(new_balloon);
  new_balloon->Show();
  panel_->Add(new_balloon);

  // There may be no listener in a unit test.
  if (space_change_listener_)
    space_change_listener_->OnBalloonSpaceChanged();
}

bool BalloonCollectionImpl::Remove(const Notification& notification) {
  Balloons::iterator iter;
  for (iter = balloons_.begin(); iter != balloons_.end(); ++iter) {
    if (notification.IsSame((*iter)->notification())) {
      // Balloon.CloseByScript() will cause OnBalloonClosed() to be called on
      // this object, which will remove it from the collection and free it.
      (*iter)->CloseByScript();
      return true;
    }
  }
  return false;
}

bool BalloonCollectionImpl::HasSpace() const {
  return true;
}

void BalloonCollectionImpl::ResizeBalloon(Balloon* balloon,
                                          const gfx::Size& size) {
  panel_->ResizeNotification(balloon, size);
}

void BalloonCollectionImpl::OnBalloonClosed(Balloon* source) {
  // We want to free the balloon when finished.
  scoped_ptr<Balloon> closed(source);

  panel_->Remove(source);

  for (Balloons::iterator it = balloons_.begin(); it != balloons_.end(); ++it) {
    if (*it == source) {
      balloons_.erase(it);
      break;
    }
  }
  // There may be no listener in a unit test.
  if (space_change_listener_)
    space_change_listener_->OnBalloonSpaceChanged();
}

Balloon* BalloonCollectionImpl::MakeBalloon(const Notification& notification,
                                            Profile* profile) {
  Balloon* balloon = new Balloon(notification, profile, this);
  balloon->set_view(new chromeos::BalloonViewImpl());
  return balloon;
}

}  // namespace chromeos

// static
BalloonCollection* BalloonCollection::Create() {
  return new chromeos::BalloonCollectionImpl();
}
