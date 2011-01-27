// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_collection_impl.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_host.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/ui/window_sizer.h"
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

BalloonCollectionImpl::BalloonCollectionImpl()
#if USE_OFFSETS
    : ALLOW_THIS_IN_INITIALIZER_LIST(reposition_factory_(this)),
      added_as_message_loop_observer_(false)
#endif
{

  SetPositionPreference(BalloonCollection::DEFAULT_POSITION);
}

BalloonCollectionImpl::~BalloonCollectionImpl() {
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
  int count = base_.count();
  if (count > 0 && layout_.RequiresOffsets())
    new_balloon->set_offset(base_.balloons()[count - 1]->offset());
#endif
  base_.Add(new_balloon);
  PositionBalloons(false);

  // There may be no listener in a unit test.
  if (space_change_listener_)
    space_change_listener_->OnBalloonSpaceChanged();

  // This is used only for testing.
  if (on_collection_changed_callback_.get())
    on_collection_changed_callback_->Run();
}

bool BalloonCollectionImpl::RemoveById(const std::string& id) {
  return base_.CloseById(id);
}

bool BalloonCollectionImpl::RemoveBySourceOrigin(const GURL& origin) {
  return base_.CloseAllBySourceOrigin(origin);
}

void BalloonCollectionImpl::RemoveAll() {
  base_.CloseAll();
}

bool BalloonCollectionImpl::HasSpace() const {
  int count = base_.count();
  if (count < kMinAllowedBalloonCount)
    return true;

  int max_balloon_size = 0;
  int total_size = 0;
  layout_.GetMaxLinearSize(&max_balloon_size, &total_size);

  int current_max_size = max_balloon_size * count;
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
  const Balloons& balloons = base_.balloons();
  Balloons::const_iterator it = balloons.begin();

#if USE_OFFSETS
  if (layout_.RequiresOffsets()) {
    gfx::Point offset;
    bool apply_offset = false;
    while (it != balloons.end()) {
      if (*it == source) {
        ++it;
        if (it != balloons.end()) {
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
  }
#endif

  base_.Remove(source);
  PositionBalloons(true);

  // There may be no listener in a unit test.
  if (space_change_listener_)
    space_change_listener_->OnBalloonSpaceChanged();

  // This is used only for testing.
  if (on_collection_changed_callback_.get())
    on_collection_changed_callback_->Run();
}

void BalloonCollectionImpl::PositionBalloonsInternal(bool reposition) {
  const Balloons& balloons = base_.balloons();

  layout_.RefreshSystemMetrics();
  gfx::Point origin = layout_.GetLayoutOrigin();
  for (Balloons::const_iterator it = balloons.begin();
       it != balloons.end();
       ++it) {
    gfx::Point upper_left = layout_.NextPosition((*it)->GetViewSize(), &origin);
    (*it)->SetPosition(upper_left, reposition);
  }
}

gfx::Rect BalloonCollectionImpl::GetBalloonsBoundingBox() const {
  // Start from the layout origin.
  gfx::Rect bounds = gfx::Rect(layout_.GetLayoutOrigin(), gfx::Size(0, 0));

  // For each balloon, extend the rectangle.  This approach is indifferent to
  // the orientation of the balloons.
  const Balloons& balloons = base_.balloons();
  Balloons::const_iterator iter;
  for (iter = balloons.begin(); iter != balloons.end(); ++iter) {
    gfx::Rect balloon_box = gfx::Rect((*iter)->GetPosition(),
                                      (*iter)->GetViewSize());
    bounds = bounds.Union(balloon_box);
  }

  return bounds;
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

  const Balloons& balloons = base_.balloons();
  for (Balloons::const_iterator it = balloons.begin();
       it != balloons.end();
       ++it)
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

  // All placement schemes are vertical, so we only care about height.
  *total_size = work_area_.height();
  *max_balloon_size = max_balloon_height();
}

gfx::Point BalloonCollectionImpl::Layout::GetLayoutOrigin() const {
  int x = 0;
  int y = 0;
  switch (placement_) {
    case VERTICALLY_FROM_TOP_LEFT:
      x = work_area_.x() + HorizontalEdgeMargin();
      y = work_area_.y() + VerticalEdgeMargin();
      break;
    case VERTICALLY_FROM_TOP_RIGHT:
      x = work_area_.right() - HorizontalEdgeMargin();
      y = work_area_.y() + VerticalEdgeMargin();
      break;
    case VERTICALLY_FROM_BOTTOM_LEFT:
      x = work_area_.x() + HorizontalEdgeMargin();
      y = work_area_.bottom() - VerticalEdgeMargin();
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
    case VERTICALLY_FROM_TOP_LEFT:
      x = position_iterator->x();
      y = position_iterator->y();
      position_iterator->set_y(position_iterator->y() + balloon_size.height() +
                               InterBalloonMargin());
      break;
    case VERTICALLY_FROM_TOP_RIGHT:
      x = position_iterator->x() - balloon_size.width();
      y = position_iterator->y();
      position_iterator->set_y(position_iterator->y() + balloon_size.height() +
                               InterBalloonMargin());
      break;
    case VERTICALLY_FROM_BOTTOM_LEFT:
      position_iterator->set_y(position_iterator->y() - balloon_size.height() -
                               InterBalloonMargin());
      x = position_iterator->x();
      y = position_iterator->y();
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
    case VERTICALLY_FROM_TOP_LEFT:
      x = work_area_.x() + HorizontalEdgeMargin();
      y = work_area_.y() + kBalloonMaxHeight + VerticalEdgeMargin();
      break;
    case VERTICALLY_FROM_TOP_RIGHT:
      x = work_area_.right() - kBalloonMaxWidth - HorizontalEdgeMargin();
      y = work_area_.y() + kBalloonMaxHeight + VerticalEdgeMargin();
      break;
    case VERTICALLY_FROM_BOTTOM_LEFT:
      x = work_area_.x() + HorizontalEdgeMargin();
      y = work_area_.bottom() + kBalloonMaxHeight + VerticalEdgeMargin();
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

bool BalloonCollectionImpl::Layout::RequiresOffsets() const {
  // Layout schemes that grow up from the bottom require offsets;
  // schemes that grow down do not require offsets.
  bool offsets = (placement_ == VERTICALLY_FROM_BOTTOM_LEFT ||
                  placement_ == VERTICALLY_FROM_BOTTOM_RIGHT);

#if defined(OS_MACOSX)
  // These schemes are in screen-coordinates, and top and bottom
  // are inverted on Mac.
  offsets = !offsets;
#endif

  return offsets;
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
