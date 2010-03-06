// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_

#include "base/string16.h"

class SkBitmap;

class StatusIcon {
 public:
  // Creates a new StatusIcon.
  static StatusIcon* Create();

  virtual ~StatusIcon() {}

  // Sets the image associated with this status icon.
  virtual void SetImage(const SkBitmap& image) = 0;

  // Sets the hover text for this status icon.
  virtual void SetToolTip(const string16& tool_tip) = 0;

  class StatusIconObserver {
   public:
    virtual ~StatusIconObserver() {}

    // Called when the user clicks on the system tray icon.
    virtual void OnClicked() = 0;
  };

  // Adds/removes a observer for status bar events.
  virtual void AddObserver(StatusIcon::StatusIconObserver* observer) = 0;
  virtual void RemoveObserver(StatusIcon::StatusIconObserver* observer) = 0;
};


#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
