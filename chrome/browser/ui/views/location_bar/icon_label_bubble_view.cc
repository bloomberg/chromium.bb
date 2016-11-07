// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace {

// Amount of space on either side of the separator that appears after the label.
constexpr int kSpaceBesideSeparator = 8;

}  // namespace

IconLabelBubbleView::IconLabelBubbleView(const gfx::FontList& font_list,
                                         bool elide_in_middle)
    : image_(new views::ImageView()),
      label_(new views::Label(base::string16(), font_list)) {
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
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(GetLayoutConstant(LOCATION_BAR_BUBBLE_VERTICAL_PADDING), 0)));

  // Flip the canvas in RTL so the separator is drawn on the correct side.
  EnableCanvasFlippingForRTLUI(true);
}

IconLabelBubbleView::~IconLabelBubbleView() {
}

void IconLabelBubbleView::SetLabel(const base::string16& label) {
  label_->SetText(label);
}

void IconLabelBubbleView::SetImage(const gfx::ImageSkia& image_skia) {
  image_->SetImage(image_skia);
}

bool IconLabelBubbleView::ShouldShowLabel() const {
  return label_->visible() && !label_->text().empty();
}

double IconLabelBubbleView::WidthMultiplier() const {
  return 1.0;
}

bool IconLabelBubbleView::IsShrinking() const {
  return false;
}

bool IconLabelBubbleView::OnActivate(const ui::Event& event) {
  return false;
}

gfx::Size IconLabelBubbleView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(label_->GetPreferredSize().width());
}

bool IconLabelBubbleView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN)
    return OnActivate(event);
  return false;
}

bool IconLabelBubbleView::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return OnActivate(event);
  return false;
}

void IconLabelBubbleView::Layout() {
  // Compute the image bounds. Leading and trailing padding are the same.
  int image_x = GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING);
  int bubble_trailing_padding = image_x;

  // If ShouldShowLabel() is true, then either we show a label in the
  // steady state, or we're not yet in the last portion of the animation.  In
  // these cases, we leave the leading and trailing padding alone.  If this is
  // false, however, then we're only showing the image, and either the view
  // width is the image width, or it's animating downwards and getting close to
  // it.  In these cases, we want to shrink the trailing padding first, so the
  // image slides all the way to the trailing edge before slowing or stopping;
  // then we want to shrink the leading padding down to zero.
  const int image_preferred_width = image_->GetPreferredSize().width();
  if (!ShouldShowLabel()) {
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
  label_->SetBackgroundColor(GetParentBackgroundColor());
  SchedulePaint();
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

std::unique_ptr<views::InkDropHighlight>
IconLabelBubbleView::CreateInkDropHighlight() const {
  // Only show a highlight effect when the label is empty/invisible.
  return label()->visible() ? nullptr
                            : InkDropHostView::CreateInkDropHighlight();
}

SkColor IconLabelBubbleView::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(GetTextColor());
}

SkColor IconLabelBubbleView::GetParentBackgroundColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultBackground);
}

gfx::Size IconLabelBubbleView::GetSizeForLabelWidth(int label_width) const {
  gfx::Size size(image_->GetPreferredSize());
  const bool shrinking = IsShrinking();
  // Animation continues for the last few pixels even after the label is not
  // visible in order to slide the icon into its final position. Therefore it
  // is necessary to animate |total_width| even when the background is hidden
  // as long as the animation is still shrinking.
  if (ShouldShowLabel() || shrinking) {
    // On scale factors < 2, we reserve 1 DIP for the 1 px separator.  For
    // higher scale factors, we simply take the separator px out of the
    // kSpaceBesideSeparator region before the separator, as that results in a
    // width closer to the desired gap than if we added a whole DIP for the
    // separator px.  (For scale 2, the two methods have equal error: 1 px.)
    const int separator_width = (GetScaleFactor() >= 2) ? 0 : 1;
    const int post_label_width =
        (kSpaceBesideSeparator + separator_width + GetPostSeparatorPadding());

    // |multiplier| grows from zero to one, stays equal to one and then shrinks
    // to zero again. The view width should correspondingly grow from zero to
    // fully showing both label and icon, stay there, then shrink to just large
    // enough to show the icon. We don't want to shrink all the way back to
    // zero, since this would mean the view would completely disappear and then
    // pop back to an icon after the animation finishes.
    const int max_width = GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING) +
                          image_->GetPreferredSize().width() +
                          GetInternalSpacing() + label_width + post_label_width;
    const int current_width = WidthMultiplier() * max_width;
    size.set_width(shrinking ? std::max(current_width, size.width())
                             : current_width);
  }
  return size;
}

int IconLabelBubbleView::GetInternalSpacing() const {
  return image_->GetPreferredSize().IsEmpty()
             ? 0
             : GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING);
}

int IconLabelBubbleView::GetPostSeparatorPadding() const {
  // The location bar will add LOCATION_BAR_HORIZONTAL_PADDING after us.
  return kSpaceBesideSeparator -
         GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING);
}

float IconLabelBubbleView::GetScaleFactor() const {
  const views::Widget* widget = GetWidget();
  // There may be no widget in tests, and in ash there may be no compositor if
  // the native view of the Widget doesn't have a parent.
  const ui::Compositor* compositor = widget ? widget->GetCompositor() : nullptr;
  return compositor ? compositor->device_scale_factor() : 1.0f;
}

const char* IconLabelBubbleView::GetClassName() const {
  return "IconLabelBubbleView";
}

void IconLabelBubbleView::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldShowLabel())
    return;

  const SkColor plain_text_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
  const SkColor separator_color = SkColorSetA(
      plain_text_color, color_utils::IsDark(plain_text_color) ? 0x59 : 0xCC);

  gfx::Rect bounds(GetLocalBounds());
  const int kSeparatorHeight = 16;
  bounds.Inset(GetPostSeparatorPadding(),
               (bounds.height() - kSeparatorHeight) / 2);

  // Draw the 1 px separator.
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  // Keep the separator aligned on a pixel center.
  const gfx::RectF pixel_aligned_bounds =
      gfx::ScaleRect(gfx::RectF(bounds), scale) - gfx::Vector2dF(0.5f, 0);
  canvas->DrawLine(pixel_aligned_bounds.top_right(),
                   pixel_aligned_bounds.bottom_right(), separator_color);
}
