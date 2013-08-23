// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include <string>

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/system_display.h"

namespace gfx {
class Display;
}

namespace extensions {

typedef std::vector<linked_ptr<
    api::system_display::DisplayUnitInfo> > DisplayInfo;

class DisplayInfoProvider : public SystemInfoProvider {
 public:
  typedef base::Callback<void(bool success)>
      RequestInfoCallback;
  typedef base::Callback<void(bool success, const std::string& error)>
      SetInfoCallback;

  // Gets a DisplayInfoProvider instance.
  static DisplayInfoProvider* Get();

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
      const api::system_display::DisplayProperties& info,
      const SetInfoCallback& callback);

  const DisplayInfo& display_info() const;

  static void InitializeForTesting(scoped_refptr<DisplayInfoProvider> provider);

 protected:
  DisplayInfoProvider();
  virtual ~DisplayInfoProvider();

  // The last information filled up by QueryInfo and is accessed on multiple
  // threads, but the whole class is being guarded by SystemInfoProvider base
  // class.
  //
  // |info_| is accessed on the UI thread while |is_waiting_for_completion_| is
  // false and on the sequenced worker pool while |is_waiting_for_completion_|
  // is true.
  DisplayInfo info_;

 private:
  // Overriden from SystemInfoProvider.
  // The implementation is platform specific.
  virtual bool QueryInfo() OVERRIDE;

  // Update the content of the |unit| obtained for |display| using
  // platform specific method.
  void UpdateDisplayUnitInfoForPlatform(
      const gfx::Display& display,
      extensions::api::system_display::DisplayUnitInfo* unit);

  static base::LazyInstance<scoped_refptr<DisplayInfoProvider> > provider_;

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
