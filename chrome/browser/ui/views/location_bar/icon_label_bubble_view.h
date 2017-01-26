// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/label.h"

namespace gfx {
class Canvas;
class FontList;
class ImageSkia;
}

namespace views {
class ImageView;
class Label;
}

// View used to draw a bubble, containing an icon and a label. We use this as a
// base for the classes that handle the location icon (including the EV bubble),
// tab-to-search UI, and content settings.
class IconLabelBubbleView : public views::InkDropHostView {
 public:
  static constexpr int kTrailingPaddingPreMd = 2;

  IconLabelBubbleView(const gfx::FontList& font_list, bool elide_in_middle);
  ~IconLabelBubbleView() override;

  void SetLabel(const base::string16& label);
  void SetImage(const gfx::ImageSkia& image);

  const views::ImageView* GetImageView() const { return image_; }
  views::ImageView* GetImageView() { return image_; }

 protected:
  static constexpr int kOpenTimeMS = 150;

  views::ImageView* image() { return image_; }
  const views::ImageView* image() const { return image_; }
  views::Label* label() { return label_; }
  const views::Label* label() const { return label_; }

  void set_next_element_interior_padding(int padding) {
    next_element_interior_padding_ = padding;
  }

  // Gets the color for displaying text.
  virtual SkColor GetTextColor() const = 0;

  // Returns true when the label should be visible.
  virtual bool ShouldShowLabel() const;

  // Returns a multiplier used to calculate the actual width of the view based
  // on its desired width.  This ranges from 0 for a zero-width view to 1 for a
  // full-width view and can be used to animate the width of the view.
  virtual double WidthMultiplier() const;

  // Returns true when animation is in progress and is shrinking.
  virtual bool IsShrinking() const;

  // The view has been activated by a user gesture such as spacebar. Returns
  // true if some handling was performed.
  virtual bool OnActivate(const ui::Event& event);

  // views::InkDropHostView:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;

  const gfx::FontList& font_list() const { return label_->font_list(); }

  SkColor GetParentBackgroundColor() const;

  gfx::Size GetSizeForLabelWidth(int label_width) const;

 private:
  // Amount of padding from the leading edge of the view to the leading edge of
  // the image, and from the trailing edge of the label (or image, if the label
  // is invisible) to the trailing edge of the view.
  int GetOuterPadding() const;

  // Spacing between the image and the label.
  int GetInternalSpacing() const;

  // Padding after the separator.
  int GetPostSeparatorPadding() const;

  float GetScaleFactor() const;

  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // The contents of the bubble.
  views::ImageView* image_;
  views::Label* label_;

  // The padding of the element that will be displayed after |this|. This value
  // is relevant for calculating the amount of space to reserve after the
  // separator.
  int next_element_interior_padding_ = 0;

  DISALLOW_COPY_AND_ASSIGN(IconLabelBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
