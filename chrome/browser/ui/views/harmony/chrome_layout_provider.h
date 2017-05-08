// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HARMONY_CHROME_LAYOUT_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_HARMONY_CHROME_LAYOUT_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

enum ChromeDistanceMetric {
  // Default minimum width of a button.
  DISTANCE_BUTTON_MINIMUM_WIDTH = views::VIEWS_DISTANCE_END,
  // Vertical spacing between a list of multiple controls in one column.
  DISTANCE_CONTROL_LIST_VERTICAL,
  // Margin between the edge of a dialog and the left, right, or bottom of a
  // contained button.
  DISTANCE_DIALOG_BUTTON_MARGIN,
  // Spacing between a dialog button and the content above it.
  DISTANCE_DIALOG_BUTTON_TOP,
  // Horizontal or vertical margin between the edge of a panel and the
  // contained content.
  DISTANCE_PANEL_CONTENT_MARGIN,
  // Smaller horizontal spacing between other controls that are logically
  // related.
  DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL,
  // Smaller vertical spacing between controls that are logically related.
  DISTANCE_RELATED_CONTROL_VERTICAL_SMALL,
  // Horizontal spacing between an item such as an icon or checkbox and a
  // label related to it.
  DISTANCE_RELATED_LABEL_HORIZONTAL,
  // Horizontal indent of a subsection relative to related items above, e.g.
  // checkboxes below explanatory text/headings.
  DISTANCE_SUBSECTION_HORIZONTAL_INDENT,
  // Horizontal spacing between controls that are logically unrelated.
  DISTANCE_UNRELATED_CONTROL_HORIZONTAL,
  // Larger horizontal spacing between unrelated controls.
  DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE,
  // Vertical spacing between controls that are logically unrelated.
  DISTANCE_UNRELATED_CONTROL_VERTICAL,
  // Larger vertical spacing between unrelated controls.
  DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE,
};

class ChromeLayoutProvider : public views::LayoutProvider {
 public:
  ChromeLayoutProvider() {}
  ~ChromeLayoutProvider() override {}

  static ChromeLayoutProvider* Get();
  static std::unique_ptr<views::LayoutProvider> CreateLayoutProvider();

  // views::LayoutProvider:
  int GetDistanceMetric(int metric) const override;
  const views::TypographyProvider& GetTypographyProvider() const override;

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

  // Returns whether to show the icon next to the title text on a dialog.
  virtual bool ShouldShowWindowIcon() const;

  // DEPRECATED.  Returns whether Harmony mode is enabled.
  //
  // Instead of using this, create a generic solution that works for all UI
  // types, e.g. by adding a new LayoutDistance value that means what you need.
  //
  // TODO(pkasting): Fix callers and remove this.
  virtual bool IsHarmonyMode() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLayoutProvider);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HARMONY_CHROME_LAYOUT_PROVIDER_H_
