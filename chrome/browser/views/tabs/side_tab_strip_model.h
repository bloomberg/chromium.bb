// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_MODEL_H_
#define CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_MODEL_H_

#include "base/string16.h"

namespace gfx {
class Point;
}
class SkBitmap;

// A model interface implemented by an object that can provide information
// about SideTabs in a SideTabStrip.
class SideTabStripModel {
 public:
  virtual ~SideTabStripModel() {}

  // Returns metadata about the tab at the specified index.
  virtual SkBitmap GetIcon(int index) const = 0;
  virtual string16 GetTitle(int index) const = 0;
  virtual bool IsSelected(int index) const = 0;

  // Different types of network activity for a tab. The NetworkState of a tab
  // may be used to alter the UI (e.g. show different kinds of loading
  // animations).
  enum NetworkState {
    NetworkState_None,    // no network activity.
    NetworkState_Waiting, // waiting for a connection.
    NetworkState_Loading  // connected, transferring data.
  };

  // Returns the NetworkState of the tab at the specified index.
  virtual NetworkState GetNetworkState(int index) const = 0;

  // Select the tab at the specified index in the model.
  virtual void SelectTab(int index) = 0;

  // Closes the tab at the specified index in the model.
  virtual void CloseTab(int index) = 0;

  // Shows a context menu for the tab at the specified index at the specified
  // point in screen coords.
  virtual void ShowContextMenu(int index, const gfx::Point& p) = 0;
};

#endif  // CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_MODEL_H_

