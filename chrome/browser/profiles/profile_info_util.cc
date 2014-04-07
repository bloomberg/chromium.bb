// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_util.h"

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

// Helper methods for transforming and drawing avatar icons.
namespace {

// Determine what the scaled height of the avatar icon should be for a
// specified width, to preserve the aspect ratio.
int GetScaledAvatarHeightForWidth(int width, const gfx::ImageSkia& avatar) {

  // Multiply the width by the inverted aspect ratio (height over
  // width), and then add 0.5 to ensure the int truncation rounds nicely.
  int scaled_height = width *
      ((float) avatar.height() / (float) avatar.width()) + 0.5f;
  return scaled_height;
}

// A CanvasImageSource that draws a sized and positioned avatar with an
// optional border independently of the scale factor.
class AvatarImageSource : public gfx::CanvasImageSource {
 public:
  enum AvatarPosition {
    POSITION_CENTER,
    POSITION_BOTTOM_CENTER,
  };

  enum AvatarBorder {
    BORDER_NONE,
    BORDER_NORMAL,
    BORDER_ETCHED,
  };

  AvatarImageSource(gfx::ImageSkia avatar,
                    const gfx::Size& canvas_size,
                    int width,
                    AvatarPosition position,
                    AvatarBorder border);
  virtual ~AvatarImageSource();

  // CanvasImageSource override:
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE;

 private:
  gfx::ImageSkia avatar_;
  const gfx::Size canvas_size_;
  const int width_;
  const int height_;
  const AvatarPosition position_;
  const AvatarBorder border_;

  DISALLOW_COPY_AND_ASSIGN(AvatarImageSource);
};

AvatarImageSource::AvatarImageSource(gfx::ImageSkia avatar,
                                     const gfx::Size& canvas_size,
                                     int width,
                                     AvatarPosition position,
                                     AvatarBorder border)
    : gfx::CanvasImageSource(canvas_size, false),
      canvas_size_(canvas_size),
      width_(width - profiles::kAvatarIconPadding),
      height_(GetScaledAvatarHeightForWidth(width, avatar) -
          profiles::kAvatarIconPadding),
      position_(position),
      border_(border) {
  avatar_ = gfx::ImageSkiaOperations::CreateResizedImage(
      avatar, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(width_, height_));
}

AvatarImageSource::~AvatarImageSource() {
}

void AvatarImageSource::Draw(gfx::Canvas* canvas) {
  // Center the avatar horizontally.
  int x = (canvas_size_.width() - width_) / 2;
  int y;

  if (position_ == POSITION_CENTER) {
    // Draw the avatar centered on the canvas.
    y = (canvas_size_.height() - height_) / 2;
  } else {
    // Draw the avatar on the bottom center of the canvas, leaving 1px below.
    y = canvas_size_.height() - height_ - 1;
  }

  canvas->DrawImageInt(avatar_, x, y);

  // The border should be square.
  int border_size = std::max(width_, height_);
  // Reset the x and y for the square border.
  x = (canvas_size_.width() - border_size) / 2;
  y = (canvas_size_.height() - border_size) / 2;

  if (border_ == BORDER_NORMAL) {
    // Draw a gray border on the inside of the avatar.
    SkColor border_color = SkColorSetARGB(83, 0, 0, 0);

    // Offset the rectangle by a half pixel so the border is drawn within the
    // appropriate pixels no matter the scale factor. Subtract 1 from the right
    // and bottom sizes to specify the endpoints, yielding -0.5.
    SkPath path;
    path.addRect(SkFloatToScalar(x + 0.5f),  // left
                 SkFloatToScalar(y + 0.5f),  // top
                 SkFloatToScalar(x + border_size - 0.5f),   // right
                 SkFloatToScalar(y + border_size - 0.5f));  // bottom

    SkPaint paint;
    paint.setColor(border_color);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(1));

    canvas->DrawPath(path, paint);
  } else if (border_ == BORDER_ETCHED) {
    // Give the avatar an etched look by drawing a highlight on the bottom and
    // right edges.
    SkColor shadow_color = SkColorSetARGB(83, 0, 0, 0);
    SkColor highlight_color = SkColorSetARGB(96, 255, 255, 255);

    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(1));

    SkPath path;

    // Left and top shadows. To support higher scale factors than 1, position
    // the orthogonal dimension of each line on the half-pixel to separate the
    // pixel. For a vertical line, this means adding 0.5 to the x-value.
    path.moveTo(SkFloatToScalar(x + 0.5f), SkIntToScalar(y + height_));

    // Draw up to the top-left. Stop with the y-value at a half-pixel.
    path.rLineTo(SkIntToScalar(0), SkFloatToScalar(-height_ + 0.5f));

    // Draw right to the top-right, stopping within the last pixel.
    path.rLineTo(SkFloatToScalar(width_ - 0.5f), SkIntToScalar(0));

    paint.setColor(shadow_color);
    canvas->DrawPath(path, paint);

    path.reset();

    // Bottom and right highlights. Note that the shadows own the shared corner
    // pixels, so reduce the sizes accordingly.
    path.moveTo(SkIntToScalar(x + 1), SkFloatToScalar(y + height_ - 0.5f));

    // Draw right to the bottom-right.
    path.rLineTo(SkFloatToScalar(width_ - 1.5f), SkIntToScalar(0));

    // Draw up to the top-right.
    path.rLineTo(SkIntToScalar(0), SkFloatToScalar(-height_ + 1.5f));

    paint.setColor(highlight_color);
    canvas->DrawPath(path, paint);
  }
}

}  // namespace

namespace profiles {

const int kAvatarIconWidth = 38;
const int kAvatarIconHeight = 31;
const int kAvatarIconPadding = 2;
const SkColor kAvatarTutorialBackgroundColor = SkColorSetRGB(0x42, 0x85, 0xf4);
const SkColor kAvatarTutorialContentTextColor = SkColorSetRGB(0xc6, 0xda, 0xfc);

gfx::Image GetSizedAvatarIconWithBorder(const gfx::Image& image,
                                        bool is_rectangle,
                                        int width, int height) {
  if (!is_rectangle)
    return image;

  gfx::Size size(width, height);

  // Source for a centered, sized icon with a border.
  scoped_ptr<gfx::ImageSkiaSource> source(
      new AvatarImageSource(
          *image.ToImageSkia(),
          size,
          std::min(width, height),
          AvatarImageSource::POSITION_CENTER,
          AvatarImageSource::BORDER_NORMAL));

  return gfx::Image(gfx::ImageSkia(source.release(), size));
}

gfx::Image GetAvatarIconForMenu(const gfx::Image& image,
                                bool is_rectangle) {
  return GetSizedAvatarIconWithBorder(
      image, is_rectangle, kAvatarIconWidth, kAvatarIconHeight);
}

gfx::Image GetAvatarIconForWebUI(const gfx::Image& image,
                                 bool is_rectangle) {
  if (!is_rectangle)
    return image;

  gfx::Size size(kAvatarIconWidth, kAvatarIconHeight);

  // Source for a centered, sized icon.
  scoped_ptr<gfx::ImageSkiaSource> source(
      new AvatarImageSource(
          *image.ToImageSkia(),
          size,
          std::min(kAvatarIconWidth, kAvatarIconHeight),
          AvatarImageSource::POSITION_CENTER,
          AvatarImageSource::BORDER_NONE));

  return gfx::Image(gfx::ImageSkia(source.release(), size));
}

gfx::Image GetAvatarIconForTitleBar(const gfx::Image& image,
                                    bool is_rectangle,
                                    int dst_width,
                                    int dst_height) {
  if (!is_rectangle)
    return image;

  int size = std::min(std::min(kAvatarIconWidth, kAvatarIconHeight),
                      std::min(dst_width, dst_height));
  gfx::Size dst_size(dst_width, dst_height);

  // Source for a sized icon drawn at the bottom center of the canvas,
  // with an etched border.
  scoped_ptr<gfx::ImageSkiaSource> source(
      new AvatarImageSource(
          *image.ToImageSkia(),
          dst_size,
          size,
          AvatarImageSource::POSITION_BOTTOM_CENTER,
          AvatarImageSource::BORDER_ETCHED));

  return gfx::Image(gfx::ImageSkia(source.release(), dst_size));
}

}  // namespace profiles
