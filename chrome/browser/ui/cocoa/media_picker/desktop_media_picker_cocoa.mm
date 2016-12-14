// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_cocoa.h"

#include <utility>

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_controller.h"

DesktopMediaPickerCocoa::DesktopMediaPickerCocoa() {
}

DesktopMediaPickerCocoa::~DesktopMediaPickerCocoa() {
}

void DesktopMediaPickerCocoa::Show(
    content::WebContents* web_contents,
    gfx::NativeWindow context,
    gfx::NativeWindow parent,
    const base::string16& app_name,
    const base::string16& target_name,
    std::unique_ptr<DesktopMediaList> screen_list,
    std::unique_ptr<DesktopMediaList> window_list,
    std::unique_ptr<DesktopMediaList> tab_list,
    bool request_audio,
    const DoneCallback& done_callback) {
  controller_.reset([[DesktopMediaPickerController alloc]
      initWithScreenList:std::move(screen_list)
              windowList:std::move(window_list)
                 tabList:std::move(tab_list)
                  parent:parent
                callback:done_callback
                 appName:app_name
              targetName:target_name
            requestAudio:request_audio]);
  [controller_ showWindow:nil];
}

// static
std::unique_ptr<DesktopMediaPicker> DesktopMediaPicker::Create() {
  return std::unique_ptr<DesktopMediaPicker>(new DesktopMediaPickerCocoa());
}
