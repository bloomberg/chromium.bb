// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HARMONY_LAYOUT_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_HARMONY_LAYOUT_DELEGATE_H_

#include "ui/views/layout/grid_layout.h"

class LayoutDelegate {
 public:
  enum class LayoutDistanceType {
    PANEL_VERT_MARGIN,
    RELATED_CONTROL_HORIZONTAL_SPACING,
    RELATED_CONTROL_VERTICAL_SPACING,
    RELATED_BUTTON_HORIZONTAL_SPACING,
    UNRELATED_CONTROL_VERTICAL_SPACING,
    UNRELATED_CONTROL_LARGE_VERTICAL_SPACING,
    BUTTON_HEDGE_MARGIN_NEW,
    BUTTON_VEDGE_MARGIN_NEW,
  };

  enum class DialogWidthType {
    SMALL,
    MEDIUM,
    LARGE,
  };

  LayoutDelegate() {}
  virtual ~LayoutDelegate() {}

  // Returns the active LayoutDelegate singleton, depending on UI configuration.
  // By default, this is the same instance returned by Get(), but if Harmony
  // or another UI style is enabled, this may be an instance of a LayoutDelegate
  // subclass instead.
  static LayoutDelegate* Get();

  // Returns a layout distance, indexed by |type|. These distances are in
  // device-independent units.
  virtual int GetLayoutDistance(LayoutDistanceType type) const;

  // Returns the alignment used for control labels in a GridLayout; for example,
  // in this GridLayout:
  //   ---------------------------
  //   | Label 1      Checkbox 1 |
  //   | Label 2      Checkbox 2 |
  //   ---------------------------
  // This value controls the alignment used for "Label 1" and "Label 2".
  virtual views::GridLayout::Alignment GetControlLabelGridAlignment() const;

  // Returns whether to use extra padding on dialogs. If this is false, content
  // Views for dialogs should not insert extra padding at their own edges.
  virtual bool UseExtraDialogPadding() const;

  // Returns whether Harmony mode is enabled. This method is deprecated and
  // should only be used in dire circumstances, such as when Harmony specifies a
  // different distance type than was previously used.
  virtual bool IsHarmonyMode() const;

  // Returns the preferred width for a dialog of the specified width type. If
  // there is no preferred width for |type|, returns 0.
  virtual int GetDialogPreferredWidth(DialogWidthType type) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HARMONY_LAYOUT_DELEGATE_H_
