// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_WIN_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_WIN_H_

#include "base/win/registry.h"
#include "chrome/browser/themes/theme_service.h"

// Tracks updates to the native colors on Windows 10 and calcuates the values we
// should use (which are not always what Windows uses). None of the values here
// are relevant to earlier versions of Windows.
class ThemeServiceWin : public ThemeService {
 public:
  ThemeServiceWin();
  ~ThemeServiceWin() override;

 private:
  // ThemeService:
  bool ShouldUseNativeFrame() const override;
  SkColor GetDefaultColor(int id, bool incognito) const override;

  // Returns true if colors from DWM can be used, i.e. this is a native frame
  // on Windows 10.
  bool DwmColorsAllowed() const;

  // Callback executed when |dwm_key_| is updated. This re-reads the active
  // frame color and updates |use_dwm_frame_color_|, |dwm_frame_color_| and
  // |dwm_inactive_frame_color_|.
  void OnDwmKeyUpdated();

  // Registry key containing the params that determine the DWM frame color.
  std::unique_ptr<base::win::RegKey> dwm_key_;

  // True if the frame should be colored using the DWM values here. False if the
  // default frame colors should be used instead.
  bool use_dwm_frame_color_;

  // The frame color when active.
  SkColor dwm_frame_color_;

  // The frame color when inactive.
  SkColor dwm_inactive_frame_color_;

  // The DWM accent border color, if available; white otherwise.
  SkColor dwm_accent_border_color_;

  DISALLOW_COPY_AND_ASSIGN(ThemeServiceWin);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_WIN_H_
