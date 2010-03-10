// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
#define CHROME_BROWSER_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_

#include <vector>

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/string16.h"
#include "chrome/browser/status_icons/status_icon.h"

class SkBitmap;
@class NSStatusItem;
@class StatusItemController;

class StatusIconMac : public StatusIcon {
 public:
  StatusIconMac();
  virtual ~StatusIconMac();

  // Sets the image associated with this status icon.
  virtual void SetImage(const SkBitmap& image);

  // Sets the pressed image associated with this status icon.
  virtual void SetPressedImage(const SkBitmap& image);

  // Sets the hover text for this status icon.
  virtual void SetToolTip(const string16& tool_tip);

  // Adds/removes a observer for status bar events.
  virtual void AddObserver(StatusIcon::StatusIconObserver* observer);
  virtual void RemoveObserver(StatusIcon::StatusIconObserver* observer);

  // Called back if the user clicks on the status bar icon.
  void HandleClick();

 private:
  // Getter for item_ that allows lazy initialization.
  NSStatusItem* item();
  scoped_nsobject<NSStatusItem> item_;

  scoped_nsobject<StatusItemController> controller_;
  std::vector<StatusIcon::StatusIconObserver*> observers_;
};


#endif // CHROME_BROWSER_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
