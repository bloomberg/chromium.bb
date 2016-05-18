// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_item.h"

#include <algorithm>
#include <vector>

#include "ash/material_design/material_design_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "base/auto_reset.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
static const int kWindowMargin = 30;
static const int kWindowMarginMD = 10;

// Foreground label color.
static const SkColor kLabelColor = SK_ColorWHITE;

// Label shadow color.
static const SkColor kLabelShadow = 0xB0000000;

// Vertical padding for the label, on top of it.
static const int kVerticalLabelPadding = 20;

// Horizontal padding for the label, on both sides. Used with Material Design.
static const int kHorizontalLabelPaddingMD = 4;

// Solid shadow length from the label
static const int kVerticalShadowOffset = 1;

// Amount of blur applied to the label shadow
static const int kShadowBlur = 10;

// Opacity for dimmed items.
static const float kDimmedItemOpacity = 0.5f;

// Calculates the |window| bounds after being transformed to the selector's
// space. With Material Design at most |title_height| is reserved above the
// |window|. The returned Rect is in virtual screen coordinates.
gfx::Rect GetTransformedBounds(aura::Window* window, int title_height) {
  gfx::RectF bounds(ScreenUtil::ConvertRectToScreen(window->GetRootWindow(),
                                                    window->GetTargetBounds()));
  gfx::Transform new_transform = TransformAboutPivot(
      gfx::Point(bounds.x(), bounds.y()),
      window->layer()->GetTargetTransform());
  new_transform.TransformRect(&bounds);

  // With Material Design the preview title is shown above the preview window.
  // Hide the window header for apps or browser windows with no tabs (web apps)
  // to avoid showing both the window header and the preview title.
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    gfx::RectF header_bounds(bounds);
    header_bounds.set_height(std::min(
        window->GetProperty(aura::client::kTopViewInset), title_height));
    new_transform.TransformRect(&header_bounds);
    bounds.Inset(0, header_bounds.height(), 0, 0);
  }
  return ToEnclosingRect(bounds);
}

// Convenience method to fade in a Window with predefined animation settings.
// Note: The fade in animation will occur after a delay where the delay is how
// long the lay out animations take.
void SetupFadeInAfterLayout(aura::Window* window) {
  ui::Layer* layer = window->layer();
  layer->SetOpacity(0.0f);
  ScopedOverviewAnimationSettings animation_settings(
      OverviewAnimationType::OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
      window);
  layer->SetOpacity(1.0f);
}

// An image button with a close window icon.
class OverviewCloseButton : public views::ImageButton {
 public:
  explicit OverviewCloseButton(views::ButtonListener* listener);
  ~OverviewCloseButton() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewCloseButton);
};

OverviewCloseButton::OverviewCloseButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::CustomButton::STATE_NORMAL,
           rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE));
  SetImage(views::CustomButton::STATE_HOVERED,
           rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE_H));
  SetImage(views::CustomButton::STATE_PRESSED,
           rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE_P));
}

OverviewCloseButton::~OverviewCloseButton() {
}

}  // namespace

WindowSelectorItem::OverviewLabelButton::OverviewLabelButton(
    views::ButtonListener* listener,
    const base::string16& text)
    : LabelButton(listener, text),
      top_padding_(0) {
}

WindowSelectorItem::OverviewLabelButton::~OverviewLabelButton() {
}

gfx::Rect WindowSelectorItem::OverviewLabelButton::GetChildAreaBounds() {
  gfx::Rect bounds = GetLocalBounds();
  if (ash::MaterialDesignController::IsOverviewMaterial())
    bounds.Inset(kHorizontalLabelPaddingMD, 0, kHorizontalLabelPaddingMD, 0);
  else
    bounds.Inset(0, top_padding_, 0, 0);
  return bounds;
}

WindowSelectorItem::WindowSelectorItem(aura::Window* window,
                                       WindowSelector* window_selector)
    : dimmed_(false),
      root_window_(window->GetRootWindow()),
      transform_window_(window),
      in_bounds_update_(false),
      window_label_button_view_(nullptr),
      close_button_(new OverviewCloseButton(this)),
      window_selector_(window_selector) {
  const bool material = ash::MaterialDesignController::IsOverviewMaterial();
  CreateWindowLabel(window->title());
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = material ? views::Widget::InitParams::OPAQUE_WINDOW
                            : views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = Shell::GetContainer(root_window_,
                                      kShellWindowId_OverlayContainer);
  close_button_widget_.set_focus_on_creation(false);
  close_button_widget_.Init(params);
  close_button_->SetVisible(false);
  close_button_widget_.SetContentsView(close_button_);
  close_button_widget_.SetSize(close_button_->GetPreferredSize());
  close_button_widget_.Show();

  gfx::Rect close_button_rect(close_button_widget_.GetNativeWindow()->bounds());
  // Align the center of the button with position (0, 0) so that the
  // translate transform does not need to take the button dimensions into
  // account.
  if (material) {
    close_button_rect.set_x(-close_button_rect.width());
    close_button_rect.set_y(-close_button_rect.height());
  } else {
    close_button_rect.set_x(-close_button_rect.width() / 2);
    close_button_rect.set_y(-close_button_rect.height() / 2);
  }
  close_button_widget_.GetNativeWindow()->SetBounds(close_button_rect);

  GetWindow()->AddObserver(this);
}

WindowSelectorItem::~WindowSelectorItem() {
  GetWindow()->RemoveObserver(this);
}

aura::Window* WindowSelectorItem::GetWindow() {
  return transform_window_.window();
}

void WindowSelectorItem::RestoreWindow() {
  transform_window_.RestoreWindow();
}

void WindowSelectorItem::ShowWindowOnExit() {
  transform_window_.ShowWindowOnExit();
}

void WindowSelectorItem::PrepareForOverview() {
  transform_window_.PrepareForOverview();
}

bool WindowSelectorItem::Contains(const aura::Window* target) const {
  return transform_window_.Contains(target);
}

void WindowSelectorItem::SetBounds(const gfx::Rect& target_bounds,
                                   OverviewAnimationType animation_type) {
  if (in_bounds_update_)
    return;
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  target_bounds_ = target_bounds;

  gfx::Rect inset_bounds(target_bounds);
  if (ash::MaterialDesignController::IsOverviewMaterial())
    inset_bounds.Inset(kWindowMarginMD, kWindowMarginMD);
  else
    inset_bounds.Inset(kWindowMargin, kWindowMargin);
  SetItemBounds(inset_bounds, animation_type);

  // SetItemBounds is called before UpdateHeaderLayout so the header can
  // properly use the updated windows bounds.
  UpdateHeaderLayout(animation_type);
  if (!ash::MaterialDesignController::IsOverviewMaterial())
    UpdateWindowLabel(target_bounds, animation_type);
}

void WindowSelectorItem::RecomputeWindowTransforms() {
  if (in_bounds_update_ || target_bounds_.IsEmpty())
    return;
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  gfx::Rect inset_bounds(target_bounds_);
  if (ash::MaterialDesignController::IsOverviewMaterial())
    inset_bounds.Inset(kWindowMarginMD, kWindowMarginMD);
  else
    inset_bounds.Inset(kWindowMargin, kWindowMargin);
  SetItemBounds(inset_bounds, OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  UpdateHeaderLayout(OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
}

void WindowSelectorItem::SendAccessibleSelectionEvent() {
  window_label_button_view_->NotifyAccessibilityEvent(
      ui::AX_EVENT_SELECTION, true);
}

void WindowSelectorItem::SetDimmed(bool dimmed) {
  dimmed_ = dimmed;
  SetOpacity(dimmed ? kDimmedItemOpacity : 1.0f);
}

void WindowSelectorItem::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == close_button_) {
    transform_window_.Close();
    return;
  }
  CHECK(sender == window_label_button_view_);
  window_selector_->SelectWindow(transform_window_.window());
}

void WindowSelectorItem::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  transform_window_.OnWindowDestroyed();
}

void WindowSelectorItem::OnWindowTitleChanged(aura::Window* window) {
  // TODO(flackr): Maybe add the new title to a vector of titles so that we can
  // filter any of the titles the window had while in the overview session.
  window_label_button_view_->SetText(window->title());
  UpdateCloseButtonAccessibilityName();
}

void WindowSelectorItem::SetItemBounds(const gfx::Rect& target_bounds,
                                       OverviewAnimationType animation_type) {
  DCHECK(root_window_ == GetWindow()->GetRootWindow());
  gfx::Rect screen_rect = transform_window_.GetTargetBoundsInScreen();

  // Avoid division by zero by ensuring screen bounds is not empty.
  gfx::Size screen_size(screen_rect.size());
  screen_size.SetToMax(gfx::Size(1, 1));
  screen_rect.set_size(screen_size);

  int top_view_inset = 0;
  int title_height = 0;
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    top_view_inset = GetWindow()->GetProperty(aura::client::kTopViewInset);
    title_height = close_button_->GetPreferredSize().height();
  }
  gfx::Rect selector_item_bounds =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          screen_rect, target_bounds, top_view_inset, title_height);
  gfx::Transform transform = ScopedTransformOverviewWindow::GetTransformForRect(
      screen_rect, selector_item_bounds);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetTransform(root_window_, transform);
  transform_window_.set_overview_transform(transform);
}

void WindowSelectorItem::SetOpacity(float opacity) {
  window_label_->GetNativeWindow()->layer()->SetOpacity(opacity);
  close_button_widget_.GetNativeWindow()->layer()->SetOpacity(opacity);

  transform_window_.SetOpacity(opacity);
}

void WindowSelectorItem::UpdateWindowLabel(
    const gfx::Rect& window_bounds,
    OverviewAnimationType animation_type) {
  // If the root window has changed, force the window label to be recreated
  // and faded in on the new root window.
  DCHECK(!window_label_ ||
         window_label_->GetNativeWindow()->GetRootWindow() == root_window_);

  if (!window_label_->IsVisible()) {
    window_label_->Show();
    SetupFadeInAfterLayout(window_label_->GetNativeWindow());
  }

  gfx::Rect converted_bounds =
      ScreenUtil::ConvertRectFromScreen(root_window_, window_bounds);
  gfx::Rect label_bounds(converted_bounds.x(), converted_bounds.y(),
                         converted_bounds.width(), converted_bounds.height());
  window_label_button_view_->set_top_padding(label_bounds.height() -
                                             kVerticalLabelPadding);
  ScopedOverviewAnimationSettings animation_settings(
      animation_type, window_label_->GetNativeWindow());

  window_label_->GetNativeWindow()->SetBounds(label_bounds);
}

void WindowSelectorItem::CreateWindowLabel(const base::string16& title) {
  const bool material = ash::MaterialDesignController::IsOverviewMaterial();
  window_label_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = material ? views::Widget::InitParams::OPAQUE_WINDOW
                            : views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window_, kShellWindowId_OverlayContainer);
  params.visible_on_all_workspaces = true;
  window_label_->set_focus_on_creation(false);
  window_label_->Init(params);
  window_label_button_view_ = new OverviewLabelButton(this, title);
  window_label_button_view_->SetBorder(views::Border::NullBorder());
  window_label_button_view_->SetEnabledTextColors(kLabelColor);
  window_label_button_view_->set_animate_on_state_change(false);
  if (material) {
    window_label_button_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  } else {
    window_label_button_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    window_label_button_view_->SetTextShadows(gfx::ShadowValues(
        1, gfx::ShadowValue(gfx::Vector2d(0, kVerticalShadowOffset),
                            kShadowBlur, kLabelShadow)));
  }
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  window_label_button_view_->SetFontList(
      bundle.GetFontList(ui::ResourceBundle::BoldFont));
  window_label_->SetContentsView(window_label_button_view_);
  if (material) {
    window_label_button_view_->SetVisible(false);
    window_label_->Show();
  }
}

void WindowSelectorItem::UpdateHeaderLayout(
    OverviewAnimationType animation_type) {
  gfx::Rect transformed_window_bounds = ScreenUtil::ConvertRectFromScreen(
      close_button_widget_.GetNativeWindow()->GetRootWindow(),
      GetTransformedBounds(GetWindow(),
                           close_button_->GetPreferredSize().height()));

  {
    if (!close_button_->visible()) {
      close_button_->SetVisible(true);
      SetupFadeInAfterLayout(close_button_widget_.GetNativeWindow());
    }
    ScopedOverviewAnimationSettings animation_settings(
        animation_type, close_button_widget_.GetNativeWindow());

    gfx::Transform close_button_transform;
    close_button_transform.Translate(transformed_window_bounds.right(),
                                     transformed_window_bounds.y());
    close_button_widget_.GetNativeWindow()->SetTransform(
        close_button_transform);
  }

  // TODO(varkha): Figure out how to draw |window_label_| opaquely over the
  // occluded header or truly hide the headers in overview mode.
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    gfx::Rect label_rect(close_button_->GetPreferredSize());
    label_rect.set_y(-label_rect.height());
    label_rect.set_width(transformed_window_bounds.width() -
                         label_rect.width());
    window_label_->GetNativeWindow()->SetBounds(label_rect);

    if (!window_label_button_view_->visible()) {
      window_label_button_view_->SetVisible(true);
      SetupFadeInAfterLayout(window_label_->GetNativeWindow());
    }
    ScopedOverviewAnimationSettings animation_settings(
        animation_type, window_label_->GetNativeWindow());
    gfx::Transform label_transform;
    label_transform.Translate(transformed_window_bounds.x(),
                              transformed_window_bounds.y());
    window_label_->GetNativeWindow()->SetTransform(label_transform);
  }
}

void WindowSelectorItem::UpdateCloseButtonAccessibilityName() {
  close_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_OVERVIEW_CLOSE_ITEM_BUTTON_ACCESSIBLE_NAME,
      GetWindow()->title()));
}

}  // namespace ash
