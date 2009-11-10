// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/skbitmap_operations.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"

// static
SkBitmap SkBitmapOperations::CreateBlendedBitmap(const SkBitmap& first,
                                                 const SkBitmap& second,
                                                 double alpha) {
  DCHECK((alpha >= 0) && (alpha <= 1));
  DCHECK(first.width() == second.width());
  DCHECK(first.height() == second.height());
  DCHECK(first.bytesPerPixel() == second.bytesPerPixel());
  DCHECK(first.config() == SkBitmap::kARGB_8888_Config);

  // Optimize for case where we won't need to blend anything.
  static const double alpha_min = 1.0 / 255;
  static const double alpha_max = 254.0 / 255;
  if (alpha < alpha_min)
    return first;
  else if (alpha > alpha_max)
    return second;

  SkAutoLockPixels lock_first(first);
  SkAutoLockPixels lock_second(second);

  SkBitmap blended;
  blended.setConfig(SkBitmap::kARGB_8888_Config, first.width(), first.height(),
                    0);
  blended.allocPixels();
  blended.eraseARGB(0, 0, 0, 0);

  double first_alpha = 1 - alpha;

  for (int y = 0; y < first.height(); ++y) {
    uint32* first_row = first.getAddr32(0, y);
    uint32* second_row = second.getAddr32(0, y);
    uint32* dst_row = blended.getAddr32(0, y);

    for (int x = 0; x < first.width(); ++x) {
      uint32 first_pixel = first_row[x];
      uint32 second_pixel = second_row[x];

      int a = static_cast<int>((SkColorGetA(first_pixel) * first_alpha) +
                               (SkColorGetA(second_pixel) * alpha));
      int r = static_cast<int>((SkColorGetR(first_pixel) * first_alpha) +
                               (SkColorGetR(second_pixel) * alpha));
      int g = static_cast<int>((SkColorGetG(first_pixel) * first_alpha) +
                               (SkColorGetG(second_pixel) * alpha));
      int b = static_cast<int>((SkColorGetB(first_pixel) * first_alpha) +
                               (SkColorGetB(second_pixel) * alpha));

      dst_row[x] = SkColorSetARGB(a, r, g, b);
    }
  }

  return blended;
}

// static
SkBitmap SkBitmapOperations::CreateMaskedBitmap(const SkBitmap& rgb,
                                                const SkBitmap& alpha) {
  DCHECK(rgb.width() == alpha.width());
  DCHECK(rgb.height() == alpha.height());
  DCHECK(rgb.bytesPerPixel() == alpha.bytesPerPixel());
  DCHECK(rgb.config() == SkBitmap::kARGB_8888_Config);
  DCHECK(alpha.config() == SkBitmap::kARGB_8888_Config);

  SkBitmap masked;
  masked.setConfig(SkBitmap::kARGB_8888_Config, rgb.width(), rgb.height(), 0);
  masked.allocPixels();
  masked.eraseARGB(0, 0, 0, 0);

  SkAutoLockPixels lock_rgb(rgb);
  SkAutoLockPixels lock_alpha(alpha);
  SkAutoLockPixels lock_masked(masked);

  for (int y = 0; y < masked.height(); ++y) {
    uint32* rgb_row = rgb.getAddr32(0, y);
    uint32* alpha_row = alpha.getAddr32(0, y);
    uint32* dst_row = masked.getAddr32(0, y);

    for (int x = 0; x < masked.width(); ++x) {
      SkColor rgb_pixel = SkUnPreMultiply::PMColorToColor(rgb_row[x]);
      int alpha = SkAlphaMul(SkColorGetA(rgb_pixel), SkColorGetA(alpha_row[x]));
      dst_row[x] = SkColorSetARGB(alpha,
                                  SkAlphaMul(SkColorGetR(rgb_pixel), alpha),
                                  SkAlphaMul(SkColorGetG(rgb_pixel), alpha),
                                  SkAlphaMul(SkColorGetB(rgb_pixel), alpha));
    }
  }

  return masked;
}

// static
SkBitmap SkBitmapOperations::CreateButtonBackground(SkColor color,
                                                    const SkBitmap& image,
                                                    const SkBitmap& mask) {
  DCHECK(image.config() == SkBitmap::kARGB_8888_Config);
  DCHECK(mask.config() == SkBitmap::kARGB_8888_Config);

  SkBitmap background;
  background.setConfig(
      SkBitmap::kARGB_8888_Config, mask.width(), mask.height(), 0);
  background.allocPixels();

  double bg_a = SkColorGetA(color);
  double bg_r = SkColorGetR(color);
  double bg_g = SkColorGetG(color);
  double bg_b = SkColorGetB(color);

  SkAutoLockPixels lock_mask(mask);
  SkAutoLockPixels lock_image(image);
  SkAutoLockPixels lock_background(background);

  for (int y = 0; y < mask.height(); ++y) {
    uint32* dst_row = background.getAddr32(0, y);
    uint32* image_row = image.getAddr32(0, y % image.height());
    uint32* mask_row = mask.getAddr32(0, y);

    for (int x = 0; x < mask.width(); ++x) {
      uint32 image_pixel = image_row[x % image.width()];

      double img_a = SkColorGetA(image_pixel);
      double img_r = SkColorGetR(image_pixel);
      double img_g = SkColorGetG(image_pixel);
      double img_b = SkColorGetB(image_pixel);

      double img_alpha = static_cast<double>(img_a) / 255.0;
      double img_inv = 1 - img_alpha;

      double mask_a = static_cast<double>(SkColorGetA(mask_row[x])) / 255.0;

      dst_row[x] = SkColorSetARGB(
          static_cast<int>(std::min(255.0, bg_a + img_a) * mask_a),
          static_cast<int>(((bg_r * img_inv) + (img_r * img_alpha)) * mask_a),
          static_cast<int>(((bg_g * img_inv) + (img_g * img_alpha)) * mask_a),
          static_cast<int>(((bg_b * img_inv) + (img_b * img_alpha)) * mask_a));
    }
  }

  return background;
}


// static
SkBitmap SkBitmapOperations::CreateHSLShiftedBitmap(
    const SkBitmap& bitmap,
    color_utils::HSL hsl_shift) {
  DCHECK(bitmap.empty() == false);
  DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);

  SkBitmap shifted;
  shifted.setConfig(SkBitmap::kARGB_8888_Config, bitmap.width(),
                    bitmap.height(), 0);
  shifted.allocPixels();
  shifted.eraseARGB(0, 0, 0, 0);
  shifted.setIsOpaque(false);

  SkAutoLockPixels lock_bitmap(bitmap);
  SkAutoLockPixels lock_shifted(shifted);

  // Loop through the pixels of the original bitmap.
  for (int y = 0; y < bitmap.height(); ++y) {
    SkPMColor* pixels = bitmap.getAddr32(0, y);
    SkPMColor* tinted_pixels = shifted.getAddr32(0, y);

    for (int x = 0; x < bitmap.width(); ++x) {
      tinted_pixels[x] = SkPreMultiplyColor(color_utils::HSLShift(
          SkUnPreMultiply::PMColorToColor(pixels[x]), hsl_shift));
    }
  }

  return shifted;
}

// static
SkBitmap SkBitmapOperations::CreateTiledBitmap(const SkBitmap& source,
                                               int src_x, int src_y,
                                               int dst_w, int dst_h) {
  DCHECK(source.getConfig() == SkBitmap::kARGB_8888_Config);

  SkBitmap cropped;
  cropped.setConfig(SkBitmap::kARGB_8888_Config, dst_w, dst_h, 0);
  cropped.allocPixels();
  cropped.eraseARGB(0, 0, 0, 0);

  SkAutoLockPixels lock_source(source);
  SkAutoLockPixels lock_cropped(cropped);

  // Loop through the pixels of the original bitmap.
  for (int y = 0; y < dst_h; ++y) {
    int y_pix = (src_y + y) % source.height();
    while (y_pix < 0)
      y_pix += source.height();

    uint32* source_row = source.getAddr32(0, y_pix);
    uint32* dst_row = cropped.getAddr32(0, y);

    for (int x = 0; x < dst_w; ++x) {
      int x_pix = (src_x + x) % source.width();
      while (x_pix < 0)
        x_pix += source.width();

      dst_row[x] = source_row[x_pix];
    }
  }

  return cropped;
}

// static
SkBitmap SkBitmapOperations::DownsampleByTwoUntilSize(const SkBitmap& bitmap,
                                                      int min_w, int min_h) {
  if ((bitmap.width() <= min_w) || (bitmap.height() <= min_h) ||
      (min_w < 0) || (min_h < 0))
    return bitmap;

  // Since bitmaps are refcounted, this copy will be fast.
  SkBitmap current = bitmap;
  while ((current.width() >= min_w * 2) && (current.height() >= min_h * 2) &&
         (current.width() > 1) && (current.height() > 1))
    current = DownsampleByTwo(current);
  return current;
}

// static
SkBitmap SkBitmapOperations::DownsampleByTwo(const SkBitmap& bitmap) {
  // Handle the nop case.
  if ((bitmap.width() <= 1) || (bitmap.height() <= 1))
    return bitmap;

  SkBitmap result;
  result.setConfig(SkBitmap::kARGB_8888_Config,
                   (bitmap.width() + 1) / 2, (bitmap.height() + 1) / 2);
  result.allocPixels();

  SkAutoLockPixels lock(bitmap);
  for (int dest_y = 0; dest_y < result.height(); ++dest_y) {
    for (int dest_x = 0; dest_x < result.width(); ++dest_x) {
      // This code is based on downsampleby2_proc32 in SkBitmap.cpp. It is very
      // clever in that it does two channels at once: alpha and green ("ag")
      // and red and blue ("rb"). Each channel gets averaged across 4 pixels
      // to get the result.
      int src_x = dest_x << 1;
      int src_y = dest_y << 1;
      const SkPMColor* cur_src = bitmap.getAddr32(src_x, src_y);
      SkPMColor tmp, ag, rb;

      // Top left pixel of the 2x2 block.
      tmp = *cur_src;
      ag = (tmp >> 8) & 0xFF00FF;
      rb = tmp & 0xFF00FF;
      if (src_x < (bitmap.width() - 1))
        ++cur_src;

      // Top right pixel of the 2x2 block.
      tmp = *cur_src;
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;
      if (src_y < (bitmap.height() - 1))
        cur_src = bitmap.getAddr32(src_x, src_y + 1);
      else
        cur_src = bitmap.getAddr32(src_x, src_y);  // Move back to the first.

      // Bottom left pixel of the 2x2 block.
      tmp = *cur_src;
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;
      if (src_x < (bitmap.width() - 1))
        ++cur_src;

      // Bottom right pixel of the 2x2 block.
      tmp = *cur_src;
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;

      // Put the channels back together, dividing each by 4 to get the average.
      // |ag| has the alpha and green channels shifted right by 8 bits from
      // there they should end up, so shifting left by 6 gives them in the
      // correct position divided by 4.
      *result.getAddr32(dest_x, dest_y) =
          ((rb >> 2) & 0xFF00FF) | ((ag << 6) & 0xFF00FF00);
    }
  }

  return result;
}

