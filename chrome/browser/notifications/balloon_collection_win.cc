// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_collection.h"

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/views/notifications/balloon_view.h"

namespace {

void GetMainScreenWorkArea(gfx::Rect* bounds) {
  DCHECK(bounds);
  RECT work_area = {0};
  if (::SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0)) {
    bounds->SetRect(work_area.left, work_area.top,
                    work_area.right, work_area.bottom);
  } else {
    // If call to ::SystemParametersInfo fails for some reason, we simply get
    // the full screen size as an alternative.
    bounds->SetRect(0, 0,
                    ::GetSystemMetrics(SM_CXSCREEN) - 1,
                    ::GetSystemMetrics(SM_CYSCREEN) - 1);
  }
}

}  // namespace

Balloon* BalloonCollectionImpl::MakeBalloon(const Notification& notification,
                                            Profile* profile) {
  Balloon* balloon = new Balloon(notification, profile, this);
  balloon->set_view(new BalloonViewImpl());
  gfx::Size size(layout_.min_balloon_width(), layout_.min_balloon_height());
  balloon->set_size(size);
  return balloon;
}

bool BalloonCollectionImpl::Layout::RefreshSystemMetrics() {
  bool changed = false;

  gfx::Rect new_work_area(work_area_.x(), work_area_.y(),
                          work_area_.width(), work_area_.height());
  GetMainScreenWorkArea(&new_work_area);
  if (!work_area_.Equals(new_work_area)) {
    work_area_.SetRect(new_work_area.x(), new_work_area.y(),
                       new_work_area.width(), new_work_area.height());
    changed = true;
  }

  return changed;
}
