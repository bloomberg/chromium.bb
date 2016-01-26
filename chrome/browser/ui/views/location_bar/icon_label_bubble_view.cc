// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"

namespace {

SkColor CalculateImageColor(gfx::ImageSkia* image) {
  // We grab the color of the middle pixel of the image, which we treat as
  // the representative color of the entire image (reasonable, given the current
  // appearance of these assets).
  const SkBitmap& bitmap(image->GetRepresentation(1.0f).sk_bitmap());
  SkAutoLockPixels pixel_lock(bitmap);
  return bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2);
}

}  // namespace

IconLabelBubbleView::IconLabelBubbleView(int contained_image,
                                         const gfx::FontList& font_list,
                                         SkColor parent_background_color,
                                         bool elide_in_middle)
    : background_painter_(nullptr),
      image_(new views::ImageView()),
      label_(new views::Label(base::string16(), font_list)),
      is_extension_icon_(false),
      parent_background_color_(parent_background_color) {
  if (contained_image) {
    image_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            contained_image));
  }

  // Disable separate hit testing for |image_|.  This prevents views treating
  // |image_| as a separate mouse hover region from |this|.
  image_->set_interactive(false);
  AddChildView(image_);

  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (elide_in_middle)
    label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  AddChildView(label_);
}

IconLabelBubbleView::~IconLabelBubbleView() {
}

void IconLabelBubbleView::SetBackgroundImageGrid(
    const int background_images[]) {
  should_show_background_ = true;
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    background_painter_.reset(
        views::Painter::CreateImageGridPainter(background_images));
    // Use the middle image of the background to represent the color of the
    // entire background.
    gfx::ImageSkia* background_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            background_images[4]);
    SetLabelBackgroundColor(CalculateImageColor(background_image));
  }
  OnNativeThemeChanged(GetNativeTheme());
}

void IconLabelBubbleView::UnsetBackgroundImageGrid() {
  should_show_background_ = false;
  SetLabelBackgroundColor(SK_ColorTRANSPARENT);
}

void IconLabelBubbleView::SetLabel(const base::string16& label) {
  label_->SetText(label);
}

void IconLabelBubbleView::SetImage(const gfx::ImageSkia& image_skia) {
  image_->SetImage(image_skia);
}

bool IconLabelBubbleView::ShouldShowBackground() const {
  return should_show_background_;
}

double IconLabelBubbleView::WidthMultiplier() const {
  return 1.0;
}

int IconLabelBubbleView::GetImageAndPaddingWidth() const {
  const int image_width = image_->GetPreferredSize().width();
  return image_width
             ? image_width + GetLayoutConstant(ICON_LABEL_VIEW_INTERNAL_PADDING)
             : 0;
}

gfx::Size IconLabelBubbleView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(label_->GetPreferredSize().width());
}

void IconLabelBubbleView::Layout() {
  // In MD mode, both extension icons and Chrome-provided icons are 16px,
  // so it's not necessary to handle them differently. TODO(estade): clean
  // this up when MD is on by default.
  bool icon_has_enough_padding =
      !is_extension_icon_ || ui::MaterialDesignController::IsModeMaterial();
  const int image_width = image_->GetPreferredSize().width();
  image_->SetBounds(std::min((width() - image_width) / 2,
                             GetBubbleOuterPadding(icon_has_enough_padding)),
                    0, image_->GetPreferredSize().width(), height());

  int pre_label_width = GetBubbleOuterPadding(true) + GetImageAndPaddingWidth();
  label_->SetBounds(pre_label_width, 0,
                    width() - pre_label_width - GetBubbleOuterPadding(false),
                    height());
}

void IconLabelBubbleView::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  label_->SetEnabledColor(GetTextColor());

  if (!ui::MaterialDesignController::IsModeMaterial())
    return;

  bool inverted = color_utils::IsDark(GetParentBackgroundColor());
  SkColor border_color = inverted ? SK_ColorWHITE : GetBorderColor();
  SkColor background_color =
      inverted ? SK_ColorWHITE : SkColorSetA(border_color, 0x13);
  set_background(new BackgroundWith1PxBorder(background_color, border_color));
  SetLabelBackgroundColor(background_color);
}

SkColor IconLabelBubbleView::GetParentBackgroundColor() const {
  return ui::MaterialDesignController::IsModeMaterial()
             ? GetNativeTheme()->GetSystemColor(
                   ui::NativeTheme::kColorId_TextfieldDefaultBackground)
             : parent_background_color_;
}

gfx::Size IconLabelBubbleView::GetSizeForLabelWidth(int width) const {
  gfx::Size size(image_->GetPreferredSize());
  if (ShouldShowBackground()) {
    const int non_label_width = GetBubbleOuterPadding(true) +
                                GetImageAndPaddingWidth() +
                                GetBubbleOuterPadding(false);
    size = gfx::Size(WidthMultiplier() * (width + non_label_width), 0);
    if (!ui::MaterialDesignController::IsModeMaterial())
      size.SetToMax(background_painter_->GetMinimumSize());
  }

  return size;
}

void IconLabelBubbleView::SetLabelBackgroundColor(
    SkColor chip_background_color) {
  // The background images are painted atop |parent_background_color_|.
  // Alpha-blend |chip_background_color| with |parent_background_color_| to
  // determine the actual color the label text will sit atop.
  // Tricky bit: We alpha blend an opaque version of |chip_background_color|
  // against |parent_background_color_| using the original image grid color's
  // alpha. This is because AlphaBlend(a, b, 255) always returns |a| unchanged
  // even if |a| is a color with non-255 alpha.
  label_->SetBackgroundColor(color_utils::AlphaBlend(
      SkColorSetA(chip_background_color, 255), GetParentBackgroundColor(),
      SkColorGetA(chip_background_color)));
}

int IconLabelBubbleView::GetBubbleOuterPadding(bool leading) const {
  if (ui::MaterialDesignController::IsModeMaterial())
    return GetBubbleOuterPaddingMd(leading);

  return GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING) -
         GetLayoutConstant(LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING) +
         (leading ? 0 : GetLayoutConstant(ICON_LABEL_VIEW_TRAILING_PADDING));
}

int IconLabelBubbleView::GetBubbleOuterPaddingMd(bool leading) const {
  // When the image is empty, leading and trailing padding are equal.
  if (image_->GetPreferredSize().IsEmpty() || !leading)
    return GetLayoutConstant(ICON_LABEL_VIEW_TRAILING_PADDING);

  // Leading padding is 2dp.
  return 2;
}

const char* IconLabelBubbleView::GetClassName() const {
  return "IconLabelBubbleView";
}

void IconLabelBubbleView::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldShowBackground())
    return;
  if (background_painter_)
    background_painter_->Paint(canvas, size());
  if (background())
    background()->Paint(canvas, this);
}
