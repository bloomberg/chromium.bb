// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_

#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/media/desktop_media_picker.h"
#include "chrome/browser/media/desktop_media_picker_model.h"
#include "chrome/common/extensions/api/desktop_capture.h"
#include "url/gurl.h"

namespace extensions {

class DesktopCaptureChooseDesktopMediaFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("desktopCapture.chooseDesktopMedia",
                             DESKTOPCAPTURE_CHOOSEDESKTOPMEDIA)

  DesktopCaptureChooseDesktopMediaFunction();

 private:
  virtual ~DesktopCaptureChooseDesktopMediaFunction();

  // ExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  void OnPickerDialogResults(content::DesktopMediaID source);

  // Origin parameter specified when chooseDesktopMedia() was called. Indicates
  // origin of the target page to use the media source chosen by the user.
  GURL origin_;

  scoped_ptr<DesktopMediaPicker> picker_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_
