// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_bridge.h"

DesktopMediaPickerBridge::DesktopMediaPickerBridge(
    id<DesktopMediaPickerObserver> observer)
    : observer_(observer) {
}

DesktopMediaPickerBridge::~DesktopMediaPickerBridge() {
}

void DesktopMediaPickerBridge::OnSourceAdded(int index) {
  [observer_ sourceAddedAtIndex:index];
}

void DesktopMediaPickerBridge::OnSourceRemoved(int index) {
  [observer_ sourceRemovedAtIndex:index];
}

void DesktopMediaPickerBridge::OnSourceMoved(int old_index, int new_index) {
  [observer_ sourceMovedFrom:old_index to:new_index];
}

void DesktopMediaPickerBridge::OnSourceNameChanged(int index) {
  [observer_ sourceNameChangedAtIndex:index];
}

void DesktopMediaPickerBridge::OnSourceThumbnailChanged(int index) {
  [observer_ sourceThumbnailChangedAtIndex:index];
}
