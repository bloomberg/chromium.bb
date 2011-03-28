// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"

class SkBitmap;

namespace ui {
class MenuModel;
}

class StatusIcon {
 public:
  StatusIcon();
  virtual ~StatusIcon();

  // Sets the image associated with this status icon.
  virtual void SetImage(const SkBitmap& image) = 0;

  // Sets the image associated with this status icon when pressed.
  virtual void SetPressedImage(const SkBitmap& image) = 0;

  // Sets the hover text for this status icon.
  virtual void SetToolTip(const string16& tool_tip) = 0;

  // Displays a notification balloon with the specified contents.
  virtual void DisplayBalloon(const string16& title,
                              const string16& contents) = 0;

  // Set the context menu for this icon. The icon takes ownership of the passed
  // context menu. Passing NULL results in no menu at all.
  void SetContextMenu(ui::MenuModel* menu);

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the user clicks on the system tray icon. Clicks that result
    // in the context menu being displayed will not be passed to this observer
    // (i.e. if there's a context menu set on this status icon, and the user
    // right clicks on the icon to display the context menu, OnClicked will not
    // be called).
    virtual void OnClicked() = 0;
  };

  // Adds/Removes an observer for clicks on the status icon. If an observer is
  // registered, then left clicks on the status icon will result in the observer
  // being called, otherwise, both left and right clicks will display the
  // context menu (if any).
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if there are registered click observers.
  bool HasObservers();

  // Dispatches a click event to the observers.
  void DispatchClickEvent();

 protected:
  // Invoked after a call to SetContextMenu() to let the platform-specific
  // subclass update the native context menu based on the new model. If NULL is
  // passed, subclass should destroy the native context menu.
  virtual void UpdatePlatformContextMenu(ui::MenuModel* model) = 0;

 private:
  ObserverList<Observer> observers_;
  // Context menu, if any.
  scoped_ptr<ui::MenuModel> context_menu_contents_;
  DISALLOW_COPY_AND_ASSIGN(StatusIcon);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
