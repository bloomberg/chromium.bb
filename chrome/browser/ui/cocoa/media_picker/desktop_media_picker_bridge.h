// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_BRIDGE_H_

#include "base/basictypes.h"
#include "chrome/browser/media/desktop_media_picker_model.h"

// Protocol corresponding to |DesktopMediaPickerModel::Observer|.
@protocol DesktopMediaPickerObserver
- (void)sourceAddedAtIndex:(int)index;
- (void)sourceRemovedAtIndex:(int)index;
- (void)sourceNameChangedAtIndex:(int)index;
- (void)sourceThumbnailChangedAtIndex:(int)index;
@end

// Provides a |DesktopMediaPickerModel::Observer| implementation that forwards
// notifications to a objective-c object implementing the
// |DesktopMediaPickerObserver| protocol.
class DesktopMediaPickerBridge : public DesktopMediaPickerModel::Observer {
 public:
  DesktopMediaPickerBridge(id<DesktopMediaPickerObserver> observer);
  virtual ~DesktopMediaPickerBridge();

  // DesktopMediaPickerModel::Observer overrides
  virtual void OnSourceAdded(int index) OVERRIDE;
  virtual void OnSourceRemoved(int index) OVERRIDE;
  virtual void OnSourceNameChanged(int index) OVERRIDE;
  virtual void OnSourceThumbnailChanged(int index) OVERRIDE;

 private:
  id<DesktopMediaPickerObserver> observer_;  // weak; owns this

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPickerBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_DESKTOP_MEDIA_PICKER_BRIDGE_H_
