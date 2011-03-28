// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/string16.h"
#include "chrome/browser/status_icons/status_icon.h"

class SkBitmap;
@class NSStatusItem;
@class StatusItemController;

class StatusIconMac : public StatusIcon {
 public:
  StatusIconMac();
  virtual ~StatusIconMac();

  // Overridden from StatusIcon
  virtual void SetImage(const SkBitmap& image);
  virtual void SetPressedImage(const SkBitmap& image);
  virtual void SetToolTip(const string16& tool_tip);
  virtual void DisplayBalloon(const string16& title, const string16& contents);

 protected:
  // Overridden from StatusIcon.
  virtual void UpdatePlatformContextMenu(ui::MenuModel* menu);

 private:
  // Getter for item_ that allows lazy initialization.
  NSStatusItem* item();
  scoped_nsobject<NSStatusItem> item_;

  scoped_nsobject<StatusItemController> controller_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconMac);
};


#endif // CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
