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
#include "ui/gfx/image/image_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop_hover.h"
#include "ui/views/border.h"
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
      builtin_leading_padding_(0),
      builtin_trailing_padding_(0),
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

  // Bubbles are given the full internal height of the location bar so that all
  // child views in the location bar have the same height. The visible height of
  // the bubble should be smaller, so use an empty border to shrink down the
  // content bounds so the background gets painted correctly.
  const int padding = GetLayoutConstant(LOCATION_BAR_BUBBLE_VERTICAL_PADDING);
  SetBorder(
      views::Border::CreateEmptyBorder(gfx::Insets(padding, 0, padding, 0)));
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

  if (ui::MaterialDesignController::IsModeMaterial()) {
    gfx::GetVisibleMargins(image_skia, &builtin_leading_padding_,
                           &builtin_trailing_padding_);
    if (base::i18n::IsRTL())
      std::swap(builtin_leading_padding_, builtin_trailing_padding_);
  }
}

bool IconLabelBubbleView::ShouldShowBackground() const {
  return should_show_background_;
}

double IconLabelBubbleView::WidthMultiplier() const {
  return 1.0;
}

bool IconLabelBubbleView::IsShrinking() const {
  return false;
}

bool IconLabelBubbleView::OnActivate() {
  return false;
}

gfx::Size IconLabelBubbleView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(label_->GetPreferredSize().width());
}

bool IconLabelBubbleView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN)
    return OnActivate();
  return false;
}

bool IconLabelBubbleView::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return OnActivate();
  return false;
}

void IconLabelBubbleView::Layout() {
  // Compute the image bounds.  In non-MD, the leading padding depends on
  // whether this is an extension icon, since extension icons and
  // Chrome-provided icons are different sizes.  In MD, these sizes are the
  // same, so it's not necessary to handle the two types differently.
  const bool icon_has_enough_padding =
      !is_extension_icon_ || ui::MaterialDesignController::IsModeMaterial();
  int image_x = GetOuterPadding(icon_has_enough_padding);
  int bubble_trailing_padding = GetOuterPadding(false);

  // If ShouldShowBackground() is true, then either we show a background in the
  // steady state, or we're not yet in the last portion of the animation.  In
  // these cases, we leave the leading and trailing padding alone; we don't want
  // to let the image overlap the edge of the background, as this looks glitchy.
  // If this is false, however, then we're only showing the image, and either
  // the view width is the image width, or it's animating downwards and getting
  // close to it.  In these cases, we want to shrink the trailing padding first,
  // so the image slides all the way to the trailing edge before slowing or
  // stopping; then we want to shrink the leading padding down to zero.
  const int image_preferred_width = image_->GetPreferredSize().width();
  if (!ShouldShowBackground()) {
    image_x = std::min(image_x, width() - image_preferred_width);
    bubble_trailing_padding = std::min(
        bubble_trailing_padding, width() - image_preferred_width - image_x);
  }

  // Now that we've computed the padding values, give the image all the
  // remaining width.  This will be less than the image's preferred width during
  // the first portion of the animation; during the very beginning there may not
  // be enough room to show the image at all.
  const int image_width =
      std::min(image_preferred_width,
               std::max(0, width() - image_x - bubble_trailing_padding));
  image_->SetBounds(image_x, 0, image_width, height());

  // Compute the label bounds.  The label gets whatever size is left over after
  // accounting for the preferred image width and padding amounts.  Note that if
  // the label has zero size it doesn't actually matter what we compute its X
  // value to be, since it won't be visible.
  const int label_x = image_x + image_width + GetInternalSpacing();
  const int label_width =
      std::max(0, width() - label_x - bubble_trailing_padding);
  label_->SetBounds(label_x, 0, label_width, height());
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

void IconLabelBubbleView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  image()->SetPaintToLayer(true);
  image()->layer()->SetFillsBoundsOpaquely(false);
  InkDropHostView::AddInkDropLayer(ink_drop_layer);
}

void IconLabelBubbleView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  InkDropHostView::RemoveInkDropLayer(ink_drop_layer);
  image()->SetPaintToLayer(false);
}

std::unique_ptr<views::InkDropHover> IconLabelBubbleView::CreateInkDropHover()
    const {
  // Location bar views don't show hover effect.
  return nullptr;
}

SkColor IconLabelBubbleView::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(GetTextColor());
}

SkColor IconLabelBubbleView::GetParentBackgroundColor() const {
  return ui::MaterialDesignController::IsModeMaterial()
             ? GetNativeTheme()->GetSystemColor(
                   ui::NativeTheme::kColorId_TextfieldDefaultBackground)
             : parent_background_color_;
}

gfx::Size IconLabelBubbleView::GetSizeForLabelWidth(int label_width) const {
  gfx::Size size(image_->GetPreferredSize());
  const bool shrinking = IsShrinking();
  // Animation continues for the last few pixels even after the label is not
  // visible in order to slide the icon into its final position. Therefore it
  // is necessary to animate |total_width| even when the background is hidden
  // as long as the animation is still shrinking.
  if (ShouldShowBackground() || shrinking) {
    // |multiplier| grows from zero to one, stays equal to one and then shrinks
    // to zero again. The view width should correspondingly grow from zero to
    // fully showing both label and icon, stay there, then shrink to just large
    // enough to show the icon. We don't want to shrink all the way back to
    // zero, since this would mean the view would completely disappear and then
    // pop back to an icon after the animation finishes.
    const int max_width = MinimumWidthForImageWithBackgroundShown() +
                          GetInternalSpacing() + label_width;
    const int current_width = WidthMultiplier() * max_width;
    size.set_width(shrinking ? std::max(current_width, size.width())
                             : current_width);
  }
  return size;
}

int IconLabelBubbleView::MinimumWidthForImageWithBackgroundShown() const {
  return GetOuterPadding(true) + image_->GetPreferredSize().width() +
         GetOuterPadding(false);
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

int IconLabelBubbleView::GetOuterPadding(bool leading) const {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    // The apparent leading and trailing padding should be equal, so we need to
    // subtract the amount of built-in padding in the image.  This will mean
    // that the actual padding + the padding inside the image add up to the same
    // amount of padding as on the trailing edge of the bubble.
    return GetLayoutConstant(ICON_LABEL_VIEW_TRAILING_PADDING) -
           (leading ? builtin_leading_padding_ : 0);
  }

  return GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING) -
         GetLayoutConstant(LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING) +
         (leading ? 0 : GetLayoutConstant(ICON_LABEL_VIEW_TRAILING_PADDING));
}

int IconLabelBubbleView::GetInternalSpacing() const {
  return image_->GetPreferredSize().IsEmpty()
             ? 0
             : (GetLayoutConstant(ICON_LABEL_VIEW_INTERNAL_SPACING) -
                builtin_trailing_padding_);
}

const char* IconLabelBubbleView::GetClassName() const {
  return "IconLabelBubbleView";
}

void IconLabelBubbleView::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldShowBackground())
    return;
  if (background_painter_) {
    views::Painter::PaintPainterAt(canvas, background_painter_.get(),
                                   GetContentsBounds());
  }
  if (background())
    background()->Paint(canvas, this);
}
