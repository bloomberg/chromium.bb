// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include <string>

#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/system_info_display.h"

namespace extensions {

typedef std::vector<linked_ptr<
    api::system_info_display::DisplayUnitInfo> > DisplayInfo;

class DisplayInfoProvider : public SystemInfoProvider<DisplayInfo> {
 public:
  typedef base::Callback<void(const DisplayInfo& info, bool success)>
      RequestInfoCallback;
  typedef base::Callback<void(bool success, const std::string& error)>
      SetInfoCallback;

  // Gets a DisplayInfoProvider instance.
  static DisplayInfoProvider* GetProvider();

  // Starts request for the display info, redirecting the request to a worker
  // thread if needed (using SystemInfoProvider<DisplayInfo>::StartQuery()).
  // The callback will be called asynchronously.
  // The implementation is platform specific.
  void RequestInfo(const RequestInfoCallback& callback);

  // Updates the display parameters for a display with the provided id.
  // The parameters are updated according to content of |info|. After the
  // display is updated, callback is called with the success status, and error
  // message. If the method succeeds, the error message is empty.
  // The callback will be called asynchronously.
  // This functionality is exposed only on ChromeOS.
  virtual void SetInfo(
      const std::string& display_id,
      const api::system_info_display::DisplayProperties& info,
      const SetInfoCallback& callback);

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
