// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_collection_impl.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_host.h"
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

// Delay from the mouse leaving the balloon collection before
// there is a relayout, in milliseconds.
const int kRepositionDelay = 300;

}  // namespace

// static
// Note that on MacOS, since the coordinate system is inverted vertically from
// the others, this actually produces notifications coming from the TOP right,
// which is what is desired.
BalloonCollectionImpl::Layout::Placement
    BalloonCollectionImpl::Layout::placement_ =
        Layout::VERTICALLY_FROM_BOTTOM_RIGHT;

BalloonCollectionImpl::BalloonCollectionImpl()
#if USE_OFFSETS
    : ALLOW_THIS_IN_INITIALIZER_LIST(reposition_factory_(this)),
      added_as_message_loop_observer_(false)
#endif
{
}

BalloonCollectionImpl::~BalloonCollectionImpl() {
  STLDeleteElements(&balloons_);
}

void BalloonCollectionImpl::Add(const Notification& notification,
                                Profile* profile) {
  Balloon* new_balloon = MakeBalloon(notification, profile);
  // The +1 on width is necessary because width is fixed on notifications,
  // so since we always have the max size, we would always hit the scrollbar
  // condition.  We are only interested in comparing height to maximum.
  new_balloon->set_min_scrollbar_size(gfx::Size(1 + layout_.max_balloon_width(),
                                                layout_.max_balloon_height()));
  new_balloon->SetPosition(layout_.OffScreenLocation(), false);
  new_balloon->Show();
#if USE_OFFSETS
  if (balloons_.size() > 0)
    new_balloon->set_offset(balloons_[balloons_.size() - 1]->offset());
#endif

  balloons_.push_back(new_balloon);
  PositionBalloons(false);

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
  if (count() < kMinAllowedBalloonCount)
    return true;

  int max_balloon_size = 0;
  int total_size = 0;
  layout_.GetMaxLinearSize(&max_balloon_size, &total_size);

  int current_max_size = max_balloon_size * count();
  int max_allowed_size = static_cast<int>(total_size *
                                          kPercentBalloonFillFactor);
  return current_max_size < max_allowed_size - max_balloon_size;
}

void BalloonCollectionImpl::ResizeBalloon(Balloon* balloon,
                                          const gfx::Size& size) {
  balloon->set_content_size(Layout::ConstrainToSizeLimits(size));
  PositionBalloons(true);
}

void BalloonCollectionImpl::DisplayChanged() {
  layout_.RefreshSystemMetrics();
  PositionBalloons(true);
}

void BalloonCollectionImpl::OnBalloonClosed(Balloon* source) {
  // We want to free the balloon when finished.
  scoped_ptr<Balloon> closed(source);
  Balloons::iterator it = balloons_.begin();

#if USE_OFFSETS
  gfx::Point offset;
  bool apply_offset = false;
  while (it != balloons_.end()) {
    if (*it == source) {
      it = balloons_.erase(it);
      if (it != balloons_.end()) {
        apply_offset = true;
        offset.set_y((source)->offset().y() - (*it)->offset().y() +
            (*it)->content_size().height() - source->content_size().height());
      }
    } else {
      if (apply_offset)
        (*it)->add_offset(offset);
      ++it;
    }
  }
  // Start listening for UI events so we cancel the offset when the mouse
  // leaves the balloon area.
  if (apply_offset)
    AddMessageLoopObserver();
#else
  for (; it != balloons_.end(); ++it) {
    if (*it == source) {
      balloons_.erase(it);
      break;
    }
  }
#endif

  PositionBalloons(true);

  // There may be no listener in a unit test.
  if (space_change_listener_)
    space_change_listener_->OnBalloonSpaceChanged();
}

void BalloonCollectionImpl::PositionBalloons(bool reposition) {
  layout_.RefreshSystemMetrics();
  gfx::Point origin = layout_.GetLayoutOrigin();
  for (Balloons::iterator it = balloons_.begin(); it != balloons_.end(); ++it) {
    gfx::Point upper_left = layout_.NextPosition((*it)->GetViewSize(), &origin);
    (*it)->SetPosition(upper_left, reposition);
  }
}

#if USE_OFFSETS
void BalloonCollectionImpl::AddMessageLoopObserver() {
  if (!added_as_message_loop_observer_) {
    MessageLoopForUI::current()->AddObserver(this);
    added_as_message_loop_observer_ = true;
  }
}

void BalloonCollectionImpl::RemoveMessageLoopObserver() {
  if (added_as_message_loop_observer_) {
    MessageLoopForUI::current()->RemoveObserver(this);
    added_as_message_loop_observer_ = false;
  }
}

void BalloonCollectionImpl::CancelOffsets() {
  reposition_factory_.RevokeAll();

  // Unhook from listening to all UI events.
  RemoveMessageLoopObserver();

  for (Balloons::iterator it = balloons_.begin(); it != balloons_.end(); ++it)
    (*it)->set_offset(gfx::Point(0, 0));

  PositionBalloons(true);
}

void BalloonCollectionImpl::HandleMouseMoveEvent() {
  if (!IsCursorInBalloonCollection()) {
    // Mouse has left the region.  Schedule a reposition after
    // a short delay.
    if (reposition_factory_.empty()) {
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          reposition_factory_.NewRunnableMethod(
              &BalloonCollectionImpl::CancelOffsets),
          kRepositionDelay);
    }
  } else {
    // Mouse moved back into the region.  Cancel the reposition.
    reposition_factory_.RevokeAll();
  }
}
#endif

BalloonCollectionImpl::Layout::Layout() {
  RefreshSystemMetrics();
}

void BalloonCollectionImpl::Layout::GetMaxLinearSize(int* max_balloon_size,
                                                     int* total_size) const {
  DCHECK(max_balloon_size && total_size);

  switch (placement_) {
    case VERTICALLY_FROM_TOP_RIGHT:
    case VERTICALLY_FROM_BOTTOM_RIGHT:
      *total_size = work_area_.height();
      *max_balloon_size = max_balloon_height();
      break;
    default:
      NOTREACHED();
      break;
  }
}

gfx::Point BalloonCollectionImpl::Layout::GetLayoutOrigin() const {
  int x = 0;
  int y = 0;
  switch (placement_) {
    case VERTICALLY_FROM_TOP_RIGHT:
      x = work_area_.right() - HorizontalEdgeMargin();
      y = work_area_.y() + VerticalEdgeMargin();
      break;
    case VERTICALLY_FROM_BOTTOM_RIGHT:
      x = work_area_.right() - HorizontalEdgeMargin();
      y = work_area_.bottom() - VerticalEdgeMargin();
      break;
    default:
      NOTREACHED();
      break;
  }
  return gfx::Point(x, y);
}

gfx::Point BalloonCollectionImpl::Layout::NextPosition(
    const gfx::Size& balloon_size,
    gfx::Point* position_iterator) const {
  DCHECK(position_iterator);

  int x = 0;
  int y = 0;
  switch (placement_) {
    case VERTICALLY_FROM_TOP_RIGHT:
      x = position_iterator->x() - balloon_size.width();
      y = position_iterator->y();
      position_iterator->set_y(position_iterator->y() + balloon_size.height() +
                               InterBalloonMargin());
      break;
    case VERTICALLY_FROM_BOTTOM_RIGHT:
      position_iterator->set_y(position_iterator->y() - balloon_size.height() -
                               InterBalloonMargin());
      x = position_iterator->x() - balloon_size.width();
      y = position_iterator->y();
      break;
    default:
      NOTREACHED();
      break;
  }
  return gfx::Point(x, y);
}

gfx::Point BalloonCollectionImpl::Layout::OffScreenLocation() const {
  int x = 0;
  int y = 0;
  switch (placement_) {
    case VERTICALLY_FROM_TOP_RIGHT:
      x = work_area_.right() - kBalloonMaxWidth - HorizontalEdgeMargin();
      y = work_area_.y() + kBalloonMaxHeight + VerticalEdgeMargin();
      break;
    case VERTICALLY_FROM_BOTTOM_RIGHT:
      x = work_area_.right() - kBalloonMaxWidth - HorizontalEdgeMargin();
      y = work_area_.bottom() + kBalloonMaxHeight + VerticalEdgeMargin();
      break;
    default:
      NOTREACHED();
      break;
  }
  return gfx::Point(x, y);
}

// static
gfx::Size BalloonCollectionImpl::Layout::ConstrainToSizeLimits(
    const gfx::Size& size) {
  // restrict to the min & max sizes
  return gfx::Size(
      std::max(min_balloon_width(),
               std::min(max_balloon_width(), size.width())),
      std::max(min_balloon_height(),
               std::min(max_balloon_height(), size.height())));
}

bool BalloonCollectionImpl::Layout::RefreshSystemMetrics() {
  bool changed = false;

#if defined(OS_MACOSX)
  gfx::Rect new_work_area = GetMacWorkArea();
#else
  scoped_ptr<WindowSizer::MonitorInfoProvider> info_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect new_work_area = info_provider->GetPrimaryMonitorWorkArea();
#endif
  if (!work_area_.Equals(new_work_area)) {
    work_area_.SetRect(new_work_area.x(), new_work_area.y(),
                       new_work_area.width(), new_work_area.height());
    changed = true;
  }

  return changed;
}
