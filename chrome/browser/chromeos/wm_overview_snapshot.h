// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_SNAPSHOT_H_
#define CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_SNAPSHOT_H_
#pragma once

#include "base/basictypes.h"

class Browser;
class SkBitmap;

namespace gfx {
class Size;
}

namespace views {
class ImageView;
class Widget;
}

namespace chromeos {

// WmOverviewSnapshot contains a snapshot image of the tab at the
// given index.
class WmOverviewSnapshot {
 public:
  WmOverviewSnapshot();
  ~WmOverviewSnapshot();

  void Init(const gfx::Size& size, Browser* browser, int index);

  void SetImage(const SkBitmap& image);

  void UpdateIndex(Browser* browser, int index);
  int index() const { return index_; }

  // Has the snapshot been configured? This is true after SetSnapshot
  // is invoked.
  bool configured_snapshot() const { return configured_snapshot_; }

  // This resets the configured_snapshot flag for this snapshot so it will
  // get reloaded the next time we check.
  void reload_snapshot() { configured_snapshot_ = false; }

  views::Widget* widget() { return widget_; }

 private:
  // This control is the contents view for this widget.
  views::ImageView* snapshot_view_;

  // This is the tab index of the snapshot in the associated browser.
  int index_;

  // This indicates whether or not the snapshot has been configured.
  bool configured_snapshot_;

  // Not owned, deletes itself when the underlying widget is destroyed.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(WmOverviewSnapshot);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_SNAPSHOT_H_
