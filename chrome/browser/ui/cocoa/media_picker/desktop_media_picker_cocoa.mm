// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_cocoa.h"

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_controller.h"

DesktopMediaPickerCocoa::DesktopMediaPickerCocoa() {
}

DesktopMediaPickerCocoa::~DesktopMediaPickerCocoa() {
}

void DesktopMediaPickerCocoa::Show(gfx::NativeWindow context,
                                   gfx::NativeWindow parent,
                                   const string16& app_name,
                                   scoped_ptr<DesktopMediaPickerModel> model,
                                   const DoneCallback& done_callback) {
  controller_.reset(
      [[DesktopMediaPickerController alloc] initWithModel:model.Pass()
                                                 callback:done_callback
                                                  appName:app_name]);
  [controller_ showWindow:nil];
}

// static
scoped_ptr<DesktopMediaPicker> DesktopMediaPicker::Create() {
  return scoped_ptr<DesktopMediaPicker>(new DesktopMediaPickerCocoa());
}
