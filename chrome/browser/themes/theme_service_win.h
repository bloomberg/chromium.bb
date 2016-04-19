// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_WIN_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_WIN_H_

#include "base/win/registry.h"
#include "chrome/browser/themes/theme_service.h"

class ThemeServiceWin : public ThemeService {
 public:
  ThemeServiceWin();
  ~ThemeServiceWin() override;

 private:
  // ThemeService:
  bool ShouldUseNativeFrame() const override;
  SkColor GetDefaultColor(int id, bool incognito) const override;

  // Returns true if the window frame color is determined by the DWM, i.e. this
  // is a native frame on Windows 10.
  bool ShouldUseDwmFrameColor() const;

  // Callback executed when |dwm_key_| is updated.  This re-reads the active
  // frame color and updates |dwm_frame_color_|.
  void OnDwmKeyUpdated();

  // Registry key containing the params that determine the DWM frame color.
  // This is only initialized on Windows 10.
  std::unique_ptr<base::win::RegKey> dwm_key_;

  // The DWM frame color, if available; white otherwise.
  SkColor dwm_frame_color_;

  DISALLOW_COPY_AND_ASSIGN(ThemeServiceWin);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_WIN_H_
