// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#import <Quartz/Quartz.h>

#include "base/callback.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/media/desktop_media_picker.h"
#include "chrome/browser/media/desktop_media_picker_model.h"
#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_bridge.h"

// A controller for the Desktop Media Picker. Presents the user with a list of
// media sources for screen-capturing, and reports the result.
@interface DesktopMediaPickerController
    : NSWindowController<NSWindowDelegate, DesktopMediaPickerObserver> {
 @private
  // The image browser view to present sources to the user (thumbnails and
  // names).
  IKImageBrowserView* sourceBrowser_;  // weak; owned by contentView

  // The button used to confirm the selection.
  NSButton* okButton_;  // weak; owned by contentView

  // The button used to cancel and close the dialog.
  NSButton* cancelButton_;  // weak; owned by contentView

  // Provides source information (including thumbnails) to fill up |items_| and
  // to render in |sourceBrowser_|.
  scoped_ptr<DesktopMediaPickerModel> model_;

  // To be called with the user selection.
  DesktopMediaPicker::DoneCallback doneCallback_;

  // Array of |DesktopMediaPickerItem| used as data for |sourceBrowser_|.
  base::scoped_nsobject<NSMutableArray> items_;

  // C++ bridge to use as an observer to |model_|, that forwards obj-c
  // notifications to this object.
  scoped_ptr<DesktopMediaPickerBridge> bridge_;

  // Used to create |DesktopMediaPickerItem|s with unique IDs.
  int lastImageUID_;
}

// Designated initializer.
// To show the dialog, use |NSWindowController|'s |showWindow:|.
// |callback| will be called to report the user's selection.
// |appName| will be used to format the dialog's title and the label.
- (id)initWithModel:(scoped_ptr<DesktopMediaPickerModel>)model
           callback:(const DesktopMediaPicker::DoneCallback&)callback
            appName:(const string16&)appName;

@end

#endif  // CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_CONTROLLER_H_
