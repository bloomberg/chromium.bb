// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include "chrome/browser/extensions/system_info_provider.h"
#include "chrome/common/extensions/api/system_info_display.h"

namespace extensions {

typedef std::vector<linked_ptr<
    api::system_info_display::DisplayUnitInfo> > DisplayInfo;

class DisplayInfoProvider : public SystemInfoProvider<DisplayInfo> {
 public:
  static DisplayInfoProvider* GetDisplayInfo();

  // Overriden from SystemInfoProvider<DisplayInfo>.
  virtual bool QueryInfo(DisplayInfo* info) OVERRIDE;

 protected:
  friend class SystemInfoProvider<DisplayInfo>;

  DisplayInfoProvider() {}
  virtual ~DisplayInfoProvider() {}

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_DISPLAY_INFO_PROVIDER_H_
