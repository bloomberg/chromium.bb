// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_BACKGROUND_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_BACKGROUND_H_

#include "ash/ash_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace ash {

// MediaNotificationBackground draws a custom background for the media
// notification showing the artwork clipped to a rounded rectangle faded into a
// background color.
class ASH_EXPORT MediaNotificationBackground : public views::Background {
 public:
  MediaNotificationBackground(views::View* owner,
                              int top_radius,
                              int bottom_radius,
                              double artwork_max_width_pct);

  // views::Background
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  void UpdateCornerRadius(int top_radius, int bottom_radius);
  void UpdateArtwork(const gfx::ImageSkia& image);
  void UpdateArtworkMaxWidthPct(double max_width_pct);

 private:
  friend class MediaNotificationViewTest;
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationBackgroundTest, BoundsSanityCheck);

  int GetArtworkWidth(const gfx::Size& view_size) const;
  int GetArtworkVisibleWidth(const gfx::Size& view_size) const;
  gfx::Rect GetArtworkBounds(const gfx::Rect& view_bounds) const;
  gfx::Rect GetFilledBackgroundBounds(const gfx::Rect& view_bounds) const;
  gfx::Rect GetGradientBounds(const gfx::Rect& view_bounds) const;

  // Reference to the owning view that this is a background for.
  views::View* owner_;

  int top_radius_;
  int bottom_radius_;

  gfx::ImageSkia artwork_;
  double artwork_max_width_pct_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationBackground);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_BACKGROUND_H_
