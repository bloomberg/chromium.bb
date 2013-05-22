// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SCROLLBAR_H_
#define CC_TEST_FAKE_SCROLLBAR_H_

#include "base/compiler_specific.h"
#include "cc/input/scrollbar.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class FakeScrollbar : public Scrollbar {
 public:
  FakeScrollbar();
  FakeScrollbar(bool paint, bool has_thumb, bool is_overlay);
  virtual ~FakeScrollbar();

  // Scrollbar implementation.
  virtual ScrollbarOrientation Orientation() const OVERRIDE;
  virtual gfx::Point Location() const OVERRIDE;
  virtual bool IsOverlay() const OVERRIDE;
  virtual bool HasThumb() const OVERRIDE;
  virtual int ThumbThickness() const OVERRIDE;
  virtual int ThumbLength() const OVERRIDE;
  virtual gfx::Rect TrackRect() const OVERRIDE;
  virtual void PaintPart(SkCanvas* canvas,
                         ScrollbarPart part,
                         gfx::Rect content_rect) OVERRIDE;

  void set_location(gfx::Point location) { location_ = location; }

 private:
  bool paint_;
  bool has_thumb_;
  bool is_overlay_;
  gfx::Point location_;
  SkColor fill_color_;

  DISALLOW_COPY_AND_ASSIGN(FakeScrollbar);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SCROLLBAR_H_
