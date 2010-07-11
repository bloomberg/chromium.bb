// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_

#include "base/observer_list.h"
#include "base/string16.h"

class SkBitmap;

class StatusIcon {
 public:
  StatusIcon() {}
  virtual ~StatusIcon() {}

  // Sets the image associated with this status icon.
  virtual void SetImage(const SkBitmap& image) = 0;

  // Sets the image associated with this status icon when pressed.
  virtual void SetPressedImage(const SkBitmap& image) = 0;

  // Sets the hover text for this status icon.
  virtual void SetToolTip(const string16& tool_tip) = 0;

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the user clicks on the system tray icon.
    virtual void OnClicked() = 0;
  };

  // Adds/Removes an observer for status bar events.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Dispatches a click event to the observers.
  void DispatchClickEvent();

 private:
  ObserverList<Observer> observers_;
  DISALLOW_COPY_AND_ASSIGN(StatusIcon);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
