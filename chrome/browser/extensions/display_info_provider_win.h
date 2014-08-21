// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_WIN_H_

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"

namespace extensions {

class DisplayInfoProviderWin : public DisplayInfoProvider {
 public:
  DisplayInfoProviderWin();
  virtual ~DisplayInfoProviderWin();

  // DisplayInfoProvider implementation.
  virtual bool SetInfo(const std::string& display_id,
                       const api::system_display::DisplayProperties& info,
                       std::string* error) OVERRIDE;
  virtual void UpdateDisplayUnitInfoForPlatform(
      const gfx::Display& display,
      extensions::api::system_display::DisplayUnitInfo* unit) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProviderWin);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_WIN_H_
