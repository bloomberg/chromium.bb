// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include <string>
#include <vector>

#include "chrome/common/extensions/api/system_display.h"

namespace gfx {
class Display;
}

namespace extensions {

typedef std::vector<linked_ptr<
    api::system_display::DisplayUnitInfo> > DisplayInfo;

class DisplayInfoProvider {
 public :
  static DisplayInfoProvider* Get();

  DisplayInfoProvider();
  virtual ~DisplayInfoProvider();

  // This is for tests that run in its own process (e.g. browser_tests).
  // Using this in other tests (e.g. unit_tests) will result in DCHECK failure.
  static void InitializeForTesting(DisplayInfoProvider* display_info_provider);

  DisplayInfo GetAllDisplaysInfo();

  virtual bool SetInfo(const std::string& display_id,
                       const api::system_display::DisplayProperties& info,
                       std::string* error);

 private:
  // Update the content of the |unit| obtained for |display| using
  // platform specific method.
  virtual void UpdateDisplayUnitInfoForPlatform(
      const gfx::Display& display,
      extensions::api::system_display::DisplayUnitInfo* unit);

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
