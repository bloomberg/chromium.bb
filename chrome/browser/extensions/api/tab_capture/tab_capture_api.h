// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Tab Capture API functions for accessing
// tab media streams.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_API_H_

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/tab_capture.h"

namespace extensions {

class TabCaptureCaptureFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabCapture.capture", TABCAPTURE_CAPTURE)

 protected:
  virtual ~TabCaptureCaptureFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class TabCaptureGetCapturedTabsFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabCapture.getCapturedTabs",
                             TABCAPTURE_GETCAPTUREDTABS)

 protected:
  virtual ~TabCaptureGetCapturedTabsFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_API_H_
