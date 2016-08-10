// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_WM_DIMMER_VIEW_H_
#define ASH_COMMON_SHELF_WM_DIMMER_VIEW_H_

namespace views {
class Widget;
}

namespace ash {

// An interface around a widget and view that dim shelf items slightly when a
// window is maximized and visible. See DimmerView for details. This interface
// exists to avoid dependencies between ash common shelf code and the aura-only
// implementation of DimmerView.
// TODO(jamescook): Delete this after material design ships, as MD will not
// require shelf dimming. http://crbug.com/614453
class WmDimmerView {
 public:
  // Returns the widget that holds the dimmer view.
  virtual views::Widget* GetDimmerWidget() = 0;

  // Force the dimmer to be undimmed (e.g. by an open context menu).
  virtual void ForceUndimming(bool force) = 0;

  // Returns the current alpha used by the dimming bar.
  virtual int GetDimmingAlphaForTest() = 0;

 protected:
  virtual ~WmDimmerView() {}
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_WM_DIMMER_VIEW_H_
