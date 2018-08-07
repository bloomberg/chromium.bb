// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace {

// Amount of space reserved for the separator that appears after the icon or
// label.
constexpr int kIconLabelBubbleSeparatorWidth = 1;

// Amount of space on either side of the separator that appears after the icon
// or label.
constexpr int kIconLabelBubbleSpaceBesideSeparator = 8;

// The length of the separator's fade animation. These values are empirical.
constexpr int kIconLabelBubbleFadeInDurationMs = 250;
constexpr int kIconLabelBubbleFadeOutDurationMs = 175;

}  // namespace

//////////////////////////////////////////////////////////////////
// SeparatorView class

IconLabelBubbleView::SeparatorView::SeparatorView(IconLabelBubbleView* owner) {
  DCHECK(owner);
  owner_ = owner;

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void IconLabelBubbleView::SeparatorView::OnPaint(gfx::Canvas* canvas) {
  const SkColor plain_text_color = owner_->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
  const SkColor separator_color = SkColorSetA(
      plain_text_color, color_utils::IsDark(plain_text_color) ? 0x59 : 0xCC);
  const float x = GetLocalBounds().right() - owner_->GetEndPadding() -
                  1.0f / canvas->image_scale();
  canvas->DrawLine(gfx::PointF(x, GetLocalBounds().y()),
                   gfx::PointF(x, GetLocalBounds().bottom()), separator_color);
}

void IconLabelBubbleView::SeparatorView::UpdateOpacity() {
  if (!visible())
    return;

  // When using focus rings are visible we should hide the separator instantly
  // when the IconLabelBubbleView is focused. Otherwise we should follow the
  // inkdrop.
  if (owner_->focus_ring() && owner_->HasFocus()) {
    layer()->SetOpacity(0.0f);
    return;
  }

  views::InkDrop* ink_drop = owner_->GetInkDrop();
  DCHECK(ink_drop);

  // If an inkdrop highlight or ripple is animating in or visible, the
  // separator should fade out.
  views::InkDropState state = ink_drop->GetTargetInkDropState();
  float opacity = 0.0f;
  float duration = kIconLabelBubbleFadeOutDurationMs;
  if (!ink_drop->IsHighlightFadingInOrVisible() &&
      (state == views::InkDropState::HIDDEN ||
       state == views::InkDropState::ACTION_TRIGGERED ||
       state == views::InkDropState::DEACTIVATED)) {
    opacity = 1.0f;
    duration = kIconLabelBubbleFadeInDurationMs;
  }

  if (disable_animation_for_test_) {
    layer()->SetOpacity(opacity);
  } else {
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(duration));
    animation.SetTweenType(gfx::Tween::Type::EASE_IN);
    layer()->SetOpacity(opacity);
  }
}

//////////////////////////////////////////////////////////////////
// IconLabelBubbleView class

IconLabelBubbleView::IconLabelBubbleView(const gfx::FontList& font_list)
    : Button(nullptr),
      image_(new views::ImageView()),
      label_(new views::Label(base::string16(), {font_list})),
      ink_drop_container_(new views::InkDropContainerView()),
      separator_view_(new SeparatorView(this)),
      suppress_button_release_(false) {
  // Disable separate hit testing for |image_|.  This prevents views treating
  // |image_| as a separate mouse hover region from |this|.
  image_->set_can_process_events_within_subtree(false);
  AddChildView(image_);

  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label_);

  separator_view_->SetVisible(ShouldShowSeparator());
  AddChildView(separator_view_);

  AddChildView(ink_drop_container_);
  ink_drop_container_->SetVisible(false);
  if (ui::MaterialDesignController::IsNewerMaterialUi()) {
    // Ink drop ripple opacity.
    set_ink_drop_visible_opacity(
        GetOmniboxStateAlpha(OmniboxPartState::SELECTED));
  }

  // Bubbles are given the full internal height of the location bar so that all
  // child views in the location bar have the same height. The visible height of
  // the bubble should be smaller, so use an empty border to shrink down the
  // content bounds so the background gets painted correctly.
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(GetLayoutConstant(LOCATION_BAR_BUBBLE_VERTICAL_PADDING),
                  GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left())));

  set_notify_enter_exit_on_child(true);

  // Flip the canvas in RTL so the separator is drawn on the correct side.
  separator_view_->EnableCanvasFlippingForRTLUI(true);
}

IconLabelBubbleView::~IconLabelBubbleView() {
}

void IconLabelBubbleView::InkDropAnimationStarted() {
  separator_view_->UpdateOpacity();
}

void IconLabelBubbleView::InkDropRippleAnimationEnded(
    views::InkDropState state) {}

bool IconLabelBubbleView::ShouldShowLabel() const {
  return label_->visible() && !label_->text().empty();
}

void IconLabelBubbleView::SetLabel(const base::string16& label) {
  label_->SetText(label);
  separator_view_->SetVisible(ShouldShowSeparator());
}

void IconLabelBubbleView::SetImage(const gfx::ImageSkia& image_skia) {
  image_->SetImage(image_skia);
}

void IconLabelBubbleView::OnBubbleCreated(views::Widget* bubble_widget) {
  bubble_widget->AddObserver(this);
}

bool IconLabelBubbleView::ShouldShowSeparator() const {
  return ShouldShowLabel();
}

bool IconLabelBubbleView::ShouldShowExtraEndSpace() const {
  return false;
}

double IconLabelBubbleView::WidthMultiplier() const {
  return 1.0;
}

bool IconLabelBubbleView::IsShrinking() const {
  return false;
}

bool IconLabelBubbleView::ShowBubble(const ui::Event& event) {
  return false;
}

bool IconLabelBubbleView::IsBubbleShowing() const {
  return false;
}

gfx::Size IconLabelBubbleView::CalculatePreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(label_->GetPreferredSize().width());
}

void IconLabelBubbleView::Layout() {
  // We may not have horizontal room for both the image and the trailing
  // padding. When the view is expanding (or showing-label steady state), the
  // image. When the view is contracting (or hidden-label steady state), whittle
  // away at the trailing padding instead.
  int bubble_trailing_padding = GetEndPadding();
  int image_width = image_->GetPreferredSize().width();
  const int space_shortage = image_width + bubble_trailing_padding - width();
  if (space_shortage > 0) {
    if (ShouldShowLabel())
      image_width -= space_shortage;
    else
      bubble_trailing_padding -= space_shortage;
  }
  image_->SetBounds(GetInsets().left(), 0, image_width, height());

  // Compute the label bounds. The label gets whatever size is left over after
  // accounting for the preferred image width and padding amounts. Note that if
  // the label has zero size it doesn't actually matter what we compute its X
  // value to be, since it won't be visible.
  const int label_x = image_->bounds().right() + GetInternalSpacing();
  int label_width = std::max(0, width() - label_x - bubble_trailing_padding -
                                    GetPrefixedSeparatorWidth());
  label_->SetBounds(label_x, 0, label_width, height());

  // The separator should be the same height as the icons.
  const int separator_height = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
  gfx::Rect separator_bounds(label_->bounds());
  separator_bounds.Inset(0, (separator_bounds.height() - separator_height) / 2);

  float separator_width = GetPrefixedSeparatorWidth() + GetEndPadding();
  int separator_x =
      ui::MaterialDesignController::IsRefreshUi() && label_->text().empty()
          ? image_->bounds().right()
          : label_->bounds().right();
  separator_view_->SetBounds(separator_x, separator_bounds.y(), separator_width,
                             separator_height);

  gfx::Rect ink_drop_bounds = GetLocalBounds();
  if (ShouldShowSeparator()) {
    ink_drop_bounds.set_width(ink_drop_bounds.width() - GetEndPadding());
  }

  ink_drop_container_->SetBoundsRect(ink_drop_bounds);

  if (focus_ring() && !ink_drop_bounds.IsEmpty()) {
    focus_ring()->Layout();
    int radius = ink_drop_bounds.height() / 2;
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(GetMirroredRect(ink_drop_bounds)),
                      radius, radius);
    focus_ring()->SetPath(path);
  }
}

bool IconLabelBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  suppress_button_release_ = IsBubbleShowing();
  return Button::OnMousePressed(event);
}

void IconLabelBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  label_->GetAccessibleNodeData(node_data);
  if (node_data->GetStringAttribute(ax::mojom::StringAttribute::kName)
          .empty()) {
    // Fallback name when there is no accessible name from the label.
    base::string16 tooltip_text;
    GetTooltipText(gfx::Point(), &tooltip_text);
    node_data->SetName(tooltip_text);
  }
}

void IconLabelBubbleView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (!IsMouseHovered()) {
    GetInkDrop()->AnimateToState(views::InkDropState::HIDDEN);
    GetInkDrop()->SetHovered(false);
  }
  Button::OnBoundsChanged(previous_bounds);
}

void IconLabelBubbleView::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  label_->SetEnabledColor(GetTextColor());
  label_->SetBackgroundColor(GetParentBackgroundColor());
  SchedulePaint();
}

void IconLabelBubbleView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  ink_drop_layer->SetBounds(ink_drop_container_->bounds());
  ink_drop_container_->AddInkDropLayer(ink_drop_layer);
  InstallInkDropMask(ink_drop_layer);
}

void IconLabelBubbleView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  ink_drop_container_->RemoveInkDropLayer(ink_drop_layer);
  ResetInkDropMask();
  separator_view_->UpdateOpacity();
}

std::unique_ptr<views::InkDrop> IconLabelBubbleView::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnFocus(!focus_ring());
  ink_drop->AddObserver(this);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> IconLabelBubbleView::CreateInkDropRipple()
    const {
  gfx::Point center_point = GetInkDropCenterBasedOnLastEvent();
  View::ConvertPointToTarget(this, ink_drop_container_, &center_point);
  center_point.SetToMax(ink_drop_container_->origin());
  center_point.SetToMin(ink_drop_container_->bounds().bottom_right());

  return std::make_unique<views::FloodFillInkDropRipple>(
      ink_drop_container_->size(), center_point, GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
IconLabelBubbleView::CreateInkDropHighlight() const {
  std::unique_ptr<views::InkDropHighlight> highlight =
      CreateDefaultInkDropHighlight(
          gfx::RectF(ink_drop_container_->bounds()).CenterPoint(),
          ink_drop_container_->size());
  if (ui::MaterialDesignController::IsNewerMaterialUi()) {
    highlight->set_visible_opacity(
        GetOmniboxStateAlpha(OmniboxPartState::HOVERED));
  }
  return highlight;
}

SkColor IconLabelBubbleView::GetInkDropBaseColor() const {
  const SkColor ink_color_opaque = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
  if (ui::MaterialDesignController::IsNewerMaterialUi()) {
    // Opacity of the ink drop is set elsewhere, so just use full opacity here.
    return ink_color_opaque;
  }
  return color_utils::DeriveDefaultIconColor(ink_color_opaque);
}

std::unique_ptr<views::InkDropMask> IconLabelBubbleView::CreateInkDropMask()
    const {
  if (!LocationBarView::IsRounded())
    return nullptr;
  return std::make_unique<views::RoundRectInkDropMask>(
      ink_drop_container_->size(), gfx::Insets(),
      ink_drop_container_->height() / 2.f);
}

bool IconLabelBubbleView::IsTriggerableEvent(const ui::Event& event) {
  if (event.IsMouseEvent())
    return !IsBubbleShowing() && !suppress_button_release_;

  return true;
}

bool IconLabelBubbleView::ShouldUpdateInkDropOnClickCanceled() const {
  // The click might be cancelled because the bubble is still opened. In this
  // case, the ink drop state should not be hidden by Button.
  return false;
}

void IconLabelBubbleView::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);
  OnActivate(event);
}

void IconLabelBubbleView::OnFocus() {
  separator_view_->UpdateOpacity();
  Button::OnFocus();
}

void IconLabelBubbleView::OnBlur() {
  separator_view_->UpdateOpacity();
  Button::OnBlur();
}

void IconLabelBubbleView::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
}

void IconLabelBubbleView::OnWidgetVisibilityChanged(views::Widget* widget,
                                                    bool visible) {
  // |widget| is a bubble that has just got shown / hidden.
  if (!visible)
    AnimateInkDrop(views::InkDropState::HIDDEN, nullptr /* event */);
}

SkColor IconLabelBubbleView::GetParentBackgroundColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultBackground);
}

gfx::Size IconLabelBubbleView::GetSizeForLabelWidth(int label_width) const {
  gfx::Size size(image_->GetPreferredSize());
  size.Enlarge(
      GetInsets().left() + GetPrefixedSeparatorWidth() + GetEndPadding(),
      GetInsets().height());

  const bool shrinking = IsShrinking();
  // Animation continues for the last few pixels even after the label is not
  // visible in order to slide the icon into its final position. Therefore it
  // is necessary to animate |total_width| even when the background is hidden
  // as long as the animation is still shrinking.
  if (ShouldShowLabel() || shrinking) {
    // |multiplier| grows from zero to one, stays equal to one and then shrinks
    // to zero again. The view width should correspondingly grow from zero to
    // fully showing both label and icon, stay there, then shrink to just large
    // enough to show the icon. We don't want to shrink all the way back to
    // zero, since this would mean the view would completely disappear and then
    // pop back to an icon after the animation finishes.
    const int max_width = size.width() + GetInternalSpacing() + label_width;
    const int current_width = WidthMultiplier() * max_width;
    size.set_width(shrinking ? std::max(current_width, size.width())
                             : current_width);
  }
  return size;
}

int IconLabelBubbleView::GetInternalSpacing() const {
  if (image_->GetPreferredSize().IsEmpty())
    return 0;

  // Touch Optimized, Refresh, and Touch Refresh all have custom spacing values.
  int default_spacing;
  switch (ui::MaterialDesignController::GetMode()) {
    case ui::MaterialDesignController::MATERIAL_TOUCH_OPTIMIZED:
      default_spacing = 4;
      break;
    case ui::MaterialDesignController::MATERIAL_REFRESH:
      default_spacing = 8;
      break;
    case ui::MaterialDesignController::MATERIAL_TOUCH_REFRESH:
      default_spacing = 10;
      break;
    default:
      default_spacing =
          GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
          GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left();
  }

  return default_spacing + GetExtraInternalSpacing();
}

int IconLabelBubbleView::GetExtraInternalSpacing() const {
  return 0;
}

int IconLabelBubbleView::GetPrefixedSeparatorWidth() const {
  return ShouldShowSeparator() || ShouldShowExtraEndSpace()
             ? kIconLabelBubbleSeparatorWidth +
                   kIconLabelBubbleSpaceBesideSeparator
             : 0;
}

int IconLabelBubbleView::GetEndPadding() const {
  if (ShouldShowSeparator())
    return kIconLabelBubbleSpaceBesideSeparator;
  return GetInsets().right();
}

bool IconLabelBubbleView::OnActivate(const ui::Event& event) {
  if (ShowBubble(event)) {
    AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
    return true;
  }

  AnimateInkDrop(views::InkDropState::HIDDEN, nullptr);
  return false;
}

const char* IconLabelBubbleView::GetClassName() const {
  return "IconLabelBubbleView";
}
