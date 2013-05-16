// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer_helper.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

namespace media {

ScreenCapturerHelper::ScreenCapturerHelper() :
    log_grid_size_(0) {
}

ScreenCapturerHelper::~ScreenCapturerHelper() {
}

void ScreenCapturerHelper::ClearInvalidRegion() {
  base::AutoLock auto_invalid_region_lock(invalid_region_lock_);
  invalid_region_.setEmpty();
}

void ScreenCapturerHelper::InvalidateRegion(
    const webrtc::DesktopRegion& invalid_region) {
  base::AutoLock auto_invalid_region_lock(invalid_region_lock_);
  for (webrtc::DesktopRegion::Iterator it(invalid_region);
       !it.IsAtEnd(); it.Advance()) {
    invalid_region_.op(SkIRect::MakeLTRB(it.rect().left(), it.rect().top(),
                                         it.rect().right(), it.rect().bottom()),
                       SkRegion::kUnion_Op);
  }
}

void ScreenCapturerHelper::InvalidateScreen(const webrtc::DesktopSize& size) {
  base::AutoLock auto_invalid_region_lock(invalid_region_lock_);
  invalid_region_.op(SkIRect::MakeWH(size.width(), size.height()),
                     SkRegion::kUnion_Op);
}

void ScreenCapturerHelper::TakeInvalidRegion(
    webrtc::DesktopRegion* invalid_region) {
  SkRegion sk_invalid_region;
  {
    base::AutoLock auto_invalid_region_lock(invalid_region_lock_);
    sk_invalid_region.swap(invalid_region_);
  }

  if (log_grid_size_ > 0) {
    scoped_ptr<SkRegion> expanded_region(
        ExpandToGrid(sk_invalid_region, log_grid_size_));
    sk_invalid_region.swap(*expanded_region);
    sk_invalid_region.op(SkIRect::MakeWH(size_most_recent_.width(),
                                         size_most_recent_.height()),
                         SkRegion::kIntersect_Op);
  }
  invalid_region->Clear();
  for (SkRegion::Iterator it(sk_invalid_region); !it.done(); it.next()) {
    invalid_region->AddRect(webrtc::DesktopRect::MakeLTRB(
        it.rect().left(), it.rect().top(),
        it.rect().right(), it.rect().bottom()));
  }
}

void ScreenCapturerHelper::SetLogGridSize(int log_grid_size) {
  log_grid_size_ = log_grid_size;
}

const webrtc::DesktopSize& ScreenCapturerHelper::size_most_recent() const {
  return size_most_recent_;
}

void ScreenCapturerHelper::set_size_most_recent(
    const webrtc::DesktopSize& size) {
  size_most_recent_ = size;
}

// Returns the largest multiple of |n| that is <= |x|.
// |n| must be a power of 2. |nMask| is ~(|n| - 1).
static int DownToMultiple(int x, int nMask) {
  return (x & nMask);
}

// Returns the smallest multiple of |n| that is >= |x|.
// |n| must be a power of 2. |nMask| is ~(|n| - 1).
static int UpToMultiple(int x, int n, int nMask) {
  return ((x + n - 1) & nMask);
}

scoped_ptr<SkRegion> ScreenCapturerHelper::ExpandToGrid(
    const SkRegion& region,
    int log_grid_size) {
  DCHECK(log_grid_size >= 1);
  int grid_size = 1 << log_grid_size;
  int grid_size_mask = ~(grid_size - 1);
  // Count the rects in the region.
  int rectNum = 0;
  SkRegion::Iterator iter(region);
  while (!iter.done()) {
    iter.next();
    ++rectNum;
  }
  // Expand each rect.
  scoped_ptr<SkIRect[]> rects(new SkIRect[rectNum]);
  iter.rewind();
  int rectI = 0;
  while (!iter.done()) {
    SkIRect rect = iter.rect();
    iter.next();
    int left = std::min(rect.left(), rect.right());
    int right = std::max(rect.left(), rect.right());
    int top = std::min(rect.top(), rect.bottom());
    int bottom = std::max(rect.top(), rect.bottom());
    left = DownToMultiple(left, grid_size_mask);
    right = UpToMultiple(right, grid_size, grid_size_mask);
    top = DownToMultiple(top, grid_size_mask);
    bottom = UpToMultiple(bottom, grid_size, grid_size_mask);
    rects[rectI++] = SkIRect::MakeLTRB(left, top, right, bottom);
  }
  // Make the union of the expanded rects.
  scoped_ptr<SkRegion> regionNew(new SkRegion());
  regionNew->setRects(rects.get(), rectNum);
  return regionNew.Pass();
}

}  // namespace media
