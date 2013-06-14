// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/system_info_display.h"

namespace extensions {

typedef std::vector<linked_ptr<
    api::system_info_display::DisplayUnitInfo> > DisplayInfo;

class DisplayInfoProvider : public SystemInfoProvider<DisplayInfo> {
 public:
  typedef base::Callback<void(const DisplayInfo& info, bool success)>
      RequestInfoCallback;

  // Gets a DisplayInfoProvider instance.
  static DisplayInfoProvider* GetProvider();

  // Starts request for the display info, redirecting the request to a worker
  // thread if needed (using SystemInfoProvider<DisplayInfo>::StartQuery()).
  // The callback will be called asynchronously.
  // The implementation is platform specific.
  void RequestInfo(const RequestInfoCallback& callback);

 protected:
  // Overriden from SystemInfoProvider<DisplayInfo>.
  // The implementation is platform specific.
  virtual bool QueryInfo(DisplayInfo* info) OVERRIDE;

  friend class SystemInfoProvider<DisplayInfo>;

  DisplayInfoProvider() {}
  virtual ~DisplayInfoProvider() {}

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_DISPLAY_INFO_PROVIDER_H_
