// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NTP_BACKGROUND_UTIL_H_
#define CHROME_BROWSER_UI_NTP_BACKGROUND_UTIL_H_

namespace gfx {
class Canvas;
class Rect;
class Size;
}

namespace ui {
class ThemeProvider;
}

class Profile;

class NtpBackgroundUtil {
 public:
  // Paints the NTP background on |canvas| where the theme image fills up
  // |tab_contents_height|.
  // - |area| is the area of the canvas that gets painted and also serves as the
  //   origin of the image (for top-aligned images).
  // - |tab_contents_height| is necessary for correctly painting bottom-aligned
  //   images since then the origin is the bottom of the web page.
  static void PaintBackgroundDetachedMode(Profile* profile,
                                          gfx::Canvas* canvas,
                                          const gfx::Rect& area,
                                          int tab_contents_height);

  // Paints the NTP background on |canvas| where the theme image fills up
  // the entire browser client area from top of tab to bottom of browser client
  // area.
  // - |area_in_canvas| is the area of and relative to the canvas that gets
  //   painted.
  // - |browser_client_area_size| is the size of browser client area where the
  //    entire image will be displayed
  // - |area_in_browser_client_area| is |area_in_canvas| relative to browser
  //   client area of |browser_client_area_size|.
  static void PaintBackgroundForBrowserClientArea(
      Profile* profile,
      gfx::Canvas* canvas,
      const gfx::Rect& area_in_canvas,
      const gfx::Size& browser_client_area_size,
      const gfx::Rect& area_in_browser_client);

  // Returns the x or y coordinate in |view_dimension| where a center-aligned
  // image of |image_dimension| should be placed:
  // - if dimension is width, x-pos is returned.
  // - if dimension is height, y-pos is returned.
  static int GetPlacementOfCenterAlignedImage(int view_dimension,
                                              int image_dimension);

 private:
  NtpBackgroundUtil() {}
};

#endif  // CHROME_BROWSER_UI_NTP_BACKGROUND_UTIL_H_
