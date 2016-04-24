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
class Painter;
}

// View used to draw a bubble, containing an icon and a label. We use this as a
// base for the classes that handle the location icon (including the EV bubble),
// tab-to-search UI, and content settings.
class IconLabelBubbleView : public views::InkDropHostView {
 public:
  IconLabelBubbleView(int contained_image,
                      const gfx::FontList& font_list,
                      SkColor parent_background_color,
                      bool elide_in_middle);
  ~IconLabelBubbleView() override;

  // Sets a background that paints |background_images| in a scalable grid.
  // Subclasses must call this during construction.
  void SetBackgroundImageGrid(const int background_images[]);
  void UnsetBackgroundImageGrid();

  void SetLabel(const base::string16& label);
  void SetImage(const gfx::ImageSkia& image);
  void set_is_extension_icon(bool is_extension_icon) {
    is_extension_icon_ = is_extension_icon;
  }

  const views::ImageView* GetImageView() const { return image_; }
  views::ImageView* GetImageView() { return image_; }

 protected:
  views::ImageView* image() { return image_; }
  views::Label* label() { return label_; }
  const views::Label* label() const { return label_; }

  // Gets the color for displaying text.
  virtual SkColor GetTextColor() const = 0;

  // Gets the color for the border (a more transparent version of which is used
  // for the background).
  virtual SkColor GetBorderColor() const = 0;

  // Returns true when the background should be rendered.
  virtual bool ShouldShowBackground() const;

  // Returns a multiplier used to calculate the actual width of the view based
  // on its desired width.  This ranges from 0 for a zero-width view to 1 for a
  // full-width view and can be used to animate the width of the view.
  virtual double WidthMultiplier() const;

  // Returns true when animation is in progress and is shrinking.
  virtual bool IsShrinking() const;

  // The view has been activated by a user gesture such as spacebar. Returns
  // true if some handling was performed.
  virtual bool OnActivate();

  // views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<views::InkDropHover> CreateInkDropHover() const override;
  SkColor GetInkDropBaseColor() const override;

  const gfx::FontList& font_list() const { return label_->font_list(); }

  SkColor GetParentBackgroundColor() const;

  gfx::Size GetSizeForLabelWidth(int label_width) const;

  // Returns the minimum width the view can be to show the complete image when
  // the background is showing.
  int MinimumWidthForImageWithBackgroundShown() const;

 private:
  // Sets a background color on |label_| based on |chip_background_color| and
  // the parent's bg color.
  void SetLabelBackgroundColor(SkColor chip_background_color);

  // Amount of padding from the leading edge of the view to the leading edge of
  // the image (if |leading| is true), or from the trailing edge of the label
  // (or image, if the label is invisible) to the trailing edge of the view.
  int GetOuterPadding(bool leading) const;

  // Spacing between the image and the label.
  int GetInternalSpacing() const;

  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // For painting the background. TODO(estade): remove post MD launch.
  std::unique_ptr<views::Painter> background_painter_;

  // The contents of the bubble.
  views::ImageView* image_;
  views::Label* label_;

  // How much horizontal padding (fully-transparent columns) is inside the
  // image.  These are subtracted from the desired padding values when
  // calculating the padding around the image, so that the image always appears
  // to have the same visible padding no matter what its composition is.  Only
  // used in MD.
  int builtin_leading_padding_;
  int builtin_trailing_padding_;

  bool is_extension_icon_;

  // This is only used in pre-MD. In MD, the background color is derived from
  // the native theme (so it responds to native theme updates). TODO(estade):
  // remove when MD is default.
  SkColor parent_background_color_;

  bool should_show_background_;

  DISALLOW_COPY_AND_ASSIGN(IconLabelBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
