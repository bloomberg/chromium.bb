// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_SNAPSHOT_H_
#define CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_SNAPSHOT_H_
#pragma once

#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/image_view.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"

class Browser;

namespace chromeos {

// WmOverviewSnapshot contains a snapshot image of the tab at the
// given index.
class WmOverviewSnapshot : public views::WidgetGtk {
 public:
  WmOverviewSnapshot();
  void Init(const gfx::Size& size, Browser* browser, int index);

  void SetImage(const SkBitmap& image);

  void UpdateIndex(Browser* browser, int index);
  int index() const { return index_; }

  // Returns the size of the snapshot widget.
  gfx::Size size() const {
    // TODO(beng): this should not be written as an accessor...
    return GetClientAreaScreenBounds().size();
  }

  // Has the snapshot been configured? This is true after SetSnapshot
  // is invoked.
  bool configured_snapshot() const { return configured_snapshot_; }

  // This resets the configured_snapshot flag for this snapshot so it will
  // get reloaded the next time we check.
  void reload_snapshot() { configured_snapshot_ = false; }

 private:
  // This control is the contents view for this widget.
  views::ImageView* snapshot_view_;

  // This is the tab index of the snapshot in the associated browser.
  int index_;

  // This indicates whether or not the snapshot has been configured.
  bool configured_snapshot_;

  DISALLOW_COPY_AND_ASSIGN(WmOverviewSnapshot);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_SNAPSHOT_H_
