// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_HELPER_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_HELPER_H_

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace media {

// ScreenCapturerHelper is intended to be used by an implementation of the
// ScreenCapturer interface. It maintains a thread-safe invalid region, and
// the size of the most recently captured screen, on behalf of the
// ScreenCapturer that owns it.
class MEDIA_EXPORT ScreenCapturerHelper {
 public:
  ScreenCapturerHelper();
  ~ScreenCapturerHelper();

  // Clear out the invalid region.
  void ClearInvalidRegion();

  // Invalidate the specified region.
  void InvalidateRegion(const SkRegion& invalid_region);

  // Invalidate the entire screen, of a given size.
  void InvalidateScreen(const SkISize& size);

  // Invalidate the entire screen, using the size of the most recently
  // captured screen.
  void InvalidateFullScreen();

  // Swap the given region with the stored invalid region.
  void SwapInvalidRegion(SkRegion* invalid_region);

  // Access the size of the most recently captured screen.
  const SkISize& size_most_recent() const;
  void set_size_most_recent(const SkISize& size);

  // Lossy compression can result in color values leaking between pixels in one
  // block. If part of a block changes, then unchanged parts of that block can
  // be changed in the compressed output. So we need to re-render an entire
  // block whenever part of the block changes.
  //
  // If |log_grid_size| is >= 1, then this function makes SwapInvalidRegion
  // produce an invalid region expanded so that its vertices lie on a grid of
  // size 2 ^ |log_grid_size|. The expanded region is then clipped to the size
  // of the most recently captured screen, as previously set by
  // set_size_most_recent().
  // If |log_grid_size| is <= 0, then the invalid region is not expanded.
  void SetLogGridSize(int log_grid_size);

  // Expands a region so that its vertices all lie on a grid.
  // The grid size must be >= 2, so |log_grid_size| must be >= 1.
  static scoped_ptr<SkRegion> ExpandToGrid(const SkRegion& region,
                                           int log_grid_size);

 private:
  // A region that has been manually invalidated (through InvalidateRegion).
  // These will be returned as dirty_region in the capture data during the next
  // capture.
  SkRegion invalid_region_;

  // A lock protecting |invalid_region_| across threads.
  base::Lock invalid_region_lock_;

  // The size of the most recently captured screen.
  SkISize size_most_recent_;

  // The log (base 2) of the size of the grid to which the invalid region is
  // expanded.
  // If the value is <= 0, then the invalid region is not expanded to a grid.
  int log_grid_size_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerHelper);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_HELPER_H_
