// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HARMONY_LAYOUT_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_HARMONY_LAYOUT_DELEGATE_H_

#include "ui/views/layout/grid_layout.h"

class LayoutDelegate {
 public:
  enum class Metric {
    // Padding on the left and right side of a button's label.
    BUTTON_HORIZONTAL_PADDING,
    // Default minimum width of a button.
    BUTTON_MINIMUM_WIDTH,
    // Margin between the edge of a dialog and the left, right, or bottom of a
    // contained button.
    DIALOG_BUTTON_MARGIN,
    // Minimum width of a dialog button.
    DIALOG_BUTTON_MINIMUM_WIDTH,
    // Spacing between a dialog button and the content above it.
    DIALOG_BUTTON_TOP_SPACING,
    // Horizontal or vertical margin between the edge of a dialog and the close
    // button in the upper trailing corner.
    DIALOG_CLOSE_BUTTON_MARGIN,
    // Horizontal or vertical margin between the edge of a panel and the
    // contained content.
    PANEL_CONTENT_MARGIN,
    // Horizontal spacing between buttons that are logically related, e.g.
    // for a button set.
    RELATED_BUTTON_HORIZONTAL_SPACING,
    // Horizontal spacing between other controls that are logically related.
    RELATED_CONTROL_HORIZONTAL_SPACING,
    // Vertical spacing between controls that are logically related.
    RELATED_CONTROL_VERTICAL_SPACING,
    // Horizontal spacing between an item such as an icon or checkbox and a
    // label related to it.
    RELATED_LABEL_HORIZONTAL_SPACING,
    // Horizontal indent of a subsection relative to related items above, e.g.
    // checkboxes below explanatory text/headings.
    SUBSECTION_HORIZONTAL_INDENT,
    // Horizontal spacing between controls that are logically unrelated.
    UNRELATED_CONTROL_HORIZONTAL_SPACING,
    // Larger horizontal spacing between unrelated controls.
    UNRELATED_CONTROL_HORIZONTAL_SPACING_LARGE,
    // Vertical spacing between controls that are logically unrelated.
    UNRELATED_CONTROL_VERTICAL_SPACING,
    // Larger vertical spacing between unrelated controls.
    UNRELATED_CONTROL_VERTICAL_SPACING_LARGE,
  };

  enum class DialogWidth {
    SMALL,
    MEDIUM,
    LARGE,
  };

  LayoutDelegate() {}
  virtual ~LayoutDelegate() {}

  // Returns the active LayoutDelegate singleton, depending on UI configuration.
  // This may be an instance of this class or a subclass, e.g. a
  // HarmonyLayoutDelegate.
  static LayoutDelegate* Get();

  // Returns the requested metric in DIPs.
  virtual int GetMetric(Metric metric) const;

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

  // DEPRECATED.  Returns whether Harmony mode is enabled.
  //
  // Instead of using this, create a generic solution that works for all UI
  // types, e.g. by adding a new LayoutDistance value that means what you need.
  //
  // TODO(pkasting): Fix callers and remove this.
  virtual bool IsHarmonyMode() const;

  // Returns the preferred width in DIPs for a dialog of the specified |width|.
  // May return 0 if the dialog has no preferred width.
  virtual int GetDialogPreferredWidth(DialogWidth width) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HARMONY_LAYOUT_DELEGATE_H_
