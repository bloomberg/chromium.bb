// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"

namespace gfx {
class Display;
class Screen;
}

namespace extensions {

namespace core_api {
namespace system_display {
struct DisplayProperties;
struct DisplayUnitInfo;
}
}

typedef std::vector<linked_ptr<core_api::system_display::DisplayUnitInfo> >
    DisplayInfo;

class DisplayInfoProvider {
 public:
  virtual ~DisplayInfoProvider();

  // Returns a pointer to DisplayInfoProvider or NULL if Create()
  // or InitializeForTesting() or not called yet.
  static DisplayInfoProvider* Get();

  // This is for tests that run in its own process (e.g. browser_tests).
  // Using this in other tests (e.g. unit_tests) will result in DCHECK failure.
  static void InitializeForTesting(DisplayInfoProvider* display_info_provider);

  // Updates the display with |display_id| according to |info|. Returns whether
  // the display was successfully updated. On failure, no display parameters
  // should be changed, and |error| should be set to the error string.
  virtual bool SetInfo(const std::string& display_id,
                       const core_api::system_display::DisplayProperties& info,
                       std::string* error) = 0;

  // Get the screen that is always active, which will be used for monitoring
  // display changes events.
  virtual gfx::Screen* GetActiveScreen() = 0;

  DisplayInfo GetAllDisplaysInfo();

 protected:
  DisplayInfoProvider();

 private:
  static DisplayInfoProvider* Create();

  // Update the content of the |unit| obtained for |display| using
  // platform specific method.
  virtual void UpdateDisplayUnitInfoForPlatform(
      const gfx::Display& display,
      core_api::system_display::DisplayUnitInfo* unit) = 0;

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
