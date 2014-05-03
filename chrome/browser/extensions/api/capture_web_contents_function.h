// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CAPTURE_WEB_CONTENTS_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_CAPTURE_WEB_CONTENTS_FUNCTION_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/types.h"

class SkBitmap;

namespace content {
class WebContents;
}

namespace extensions {

// Base class for capturing visibile area of a WebContents.
// This is used by both webview.captureVisibleRegion and tabs.captureVisibleTab.
class CaptureWebContentsFunction : public ChromeAsyncExtensionFunction {
 public:
  CaptureWebContentsFunction();

 protected:
  virtual ~CaptureWebContentsFunction();

  // ExtensionFunction implementation.
  virtual bool HasPermission() OVERRIDE;
  virtual bool RunAsync() OVERRIDE;

  virtual bool IsScreenshotEnabled() = 0;
  virtual content::WebContents* GetWebContentsForID(int context_id) = 0;

  enum FailureReason {
    FAILURE_REASON_UNKNOWN,
    FAILURE_REASON_ENCODING_FAILED,
    FAILURE_REASON_VIEW_INVISIBLE
  };
  virtual void OnCaptureFailure(FailureReason reason) = 0;

 private:
  typedef api::types::ImageDetails ImageDetails;

  void CopyFromBackingStoreComplete(bool succeed, const SkBitmap& bitmap);
  void OnCaptureSuccess(const SkBitmap& bitmap);

  // |context_id_| is the ID used to find the relevant WebContents. In the
  // |tabs.captureVisibleTab()| api, this represents the window-id, and in the
  // |webview.captureVisibleRegion()| api, this represents the instance-id of
  // the guest.
  int context_id_;

  // The format (JPEG vs PNG) of the resulting image.  Set in RunAsync().
  ImageDetails::Format image_format_;

  // Quality setting to use when encoding jpegs.  Set in RunAsync().
  int image_quality_;

  DISALLOW_COPY_AND_ASSIGN(CaptureWebContentsFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CAPTURE_WEB_CONTENTS_FUNCTION_H_
