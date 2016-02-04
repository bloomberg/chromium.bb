// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_COCOA_H_

#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/media/desktop_media_picker.h"

@class DesktopMediaPickerController;

// Cocoa's DesktopMediaPicker implementation.
class DesktopMediaPickerCocoa : public DesktopMediaPicker {
 public:
  DesktopMediaPickerCocoa();
  ~DesktopMediaPickerCocoa() override;

  // Overridden from DesktopMediaPicker:
  void Show(content::WebContents* web_contents,
            gfx::NativeWindow context,
            gfx::NativeWindow parent,
            const base::string16& app_name,
            const base::string16& target_name,
            scoped_ptr<DesktopMediaList> media_list,
            bool request_audio,
            const DoneCallback& done_callback) override;

 private:
  base::scoped_nsobject<DesktopMediaPickerController> controller_;
};

#endif  // CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_COCOA_H_
