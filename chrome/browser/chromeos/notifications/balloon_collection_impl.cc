// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/chromeos/notifications/balloon_view.h"
#include "chrome/browser/chromeos/notifications/notification_panel.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/window_sizer.h"
#include "gfx/rect.h"
#include "gfx/size.h"

namespace {

// Portion of the screen allotted for notifications. When notification balloons
// extend over this, no new notifications are shown until some are closed.
const double kPercentBalloonFillFactor = 0.7;

// Allow at least this number of balloons on the screen.
const int kMinAllowedBalloonCount = 2;

// Margin from the edge of the work area
const int kVerticalEdgeMargin = 5;
const int kHorizontalEdgeMargin = 5;

class NotificationMatcher {
 public:
  explicit NotificationMatcher(const Notification& notification)
      : notification_(notification) {}
  bool operator()(const Balloon* b) {
    return notification_.IsSame(b->notification());
  }
 private:
  Notification notification_;
};

}  // namespace

namespace chromeos {

BalloonCollectionImpl::BalloonCollectionImpl()
    : notification_ui_(new NotificationPanel()) {
}

BalloonCollectionImpl::~BalloonCollectionImpl() {
  // We need to remove the panel first because deleting
  // views that are not owned by parent will not remove
  // themselves from the parent.
  notification_ui_.reset();
  STLDeleteElements(&balloons_);
}

void BalloonCollectionImpl::Add(const Notification& notification,
                                Profile* profile) {
  Balloon* new_balloon = MakeBalloon(notification, profile);
  balloons_.push_back(new_balloon);
  new_balloon->Show();
  notification_ui_->Add(new_balloon);

  // There may be no listener in a unit test.
  if (space_change_listener_)
    space_change_listener_->OnBalloonSpaceChanged();
}

void BalloonCollectionImpl::AddSystemNotification(
    const Notification& notification,
    Profile* profile,
    bool sticky,
    bool control) {
  // TODO(oshima): We need to modify BallonCollection/MakeBalloon pattern
  // in order to add unit tests for system notification.
  Balloon* new_balloon = new Balloon(notification, profile, this);
  new_balloon->set_view(
      new chromeos::BalloonViewImpl(sticky, control));
  balloons_.push_back(new_balloon);
  new_balloon->Show();
  notification_ui_->Add(new_balloon);

  // There may be no listener in a unit test.
  if (space_change_listener_)
    space_change_listener_->OnBalloonSpaceChanged();
}

bool BalloonCollectionImpl::UpdateNotification(
    const Notification& notification) {
  Balloons::iterator iter = FindBalloon(notification);
  if (iter != balloons_.end()) {
    Balloon* balloon = *iter;
    balloon->Update(notification);
    notification_ui_->Update(balloon);
    return true;
  }
  return false;
}

bool BalloonCollectionImpl::Remove(const Notification& notification) {
  Balloons::iterator iter = FindBalloon(notification);
  if (iter != balloons_.end()) {
    // Balloon.CloseByScript() will cause OnBalloonClosed() to be called on
    // this object, which will remove it from the collection and free it.
    (*iter)->CloseByScript();
    return true;
  }
  return false;
}

bool BalloonCollectionImpl::HasSpace() const {
  return true;
}

void BalloonCollectionImpl::ResizeBalloon(Balloon* balloon,
                                          const gfx::Size& size) {
  notification_ui_->ResizeNotification(balloon, size);
}

void BalloonCollectionImpl::OnBalloonClosed(Balloon* source) {
  // We want to free the balloon when finished.
  scoped_ptr<Balloon> closed(source);

  notification_ui_->Remove(source);

  Balloons::iterator iter = FindBalloon(source->notification());
  if (iter != balloons_.end()) {
    balloons_.erase(iter);
  }
  // There may be no listener in a unit test.
  if (space_change_listener_)
    space_change_listener_->OnBalloonSpaceChanged();
}

Balloon* BalloonCollectionImpl::MakeBalloon(const Notification& notification,
                                            Profile* profile) {
  Balloon* new_balloon = new Balloon(notification, profile, this);
  new_balloon->set_view(new chromeos::BalloonViewImpl(false, true));
  return new_balloon;
}

std::deque<Balloon*>::iterator BalloonCollectionImpl::FindBalloon(
    const Notification& notification) {
  return std::find_if(balloons_.begin(),
                      balloons_.end(),
                      NotificationMatcher(notification));
}

}  // namespace chromeos

// static
BalloonCollection* BalloonCollection::Create() {
  return new chromeos::BalloonCollectionImpl();
}
