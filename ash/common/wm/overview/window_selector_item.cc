// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/overview/window_selector_item.h"

#include <algorithm>
#include <vector>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/overview/overview_animation_type.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings_factory.h"
#include "ash/common/wm/overview/scoped_transform_overview_window.h"
#include "ash/common/wm/overview/window_selector.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "base/auto_reset.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/transform_util.h"
#include "ui/gfx/vector_icons.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
static const int kWindowMargin = 30;
static const int kWindowMarginMD = 5;

// Cover the transformed window including the gaps between the windows with a
// transparent shield to block the input events from reaching the transformed
// window while in overview.
static const int kWindowSelectorMargin = kWindowMarginMD * 2;

// Foreground label color.
static const SkColor kLabelColor = SK_ColorWHITE;

// TODO(tdanderson): Move this to a central location.
static const SkColor kCloseButtonColor = SK_ColorWHITE;

// Label background color used with Material Design.
// TODO(varkha): Make background color conform to window header.
static const SkColor kLabelBackgroundColor = SkColorSetARGB(25, 255, 255, 255);

// Corner radius for the selection tiles used with Material Design.
static int kLabelBackgroundRadius = 2;

// Label shadow color.
static const SkColor kLabelShadow = SkColorSetARGB(176, 0, 0, 0);

// Vertical padding for the label, on top of it.
static const int kVerticalLabelPadding = 20;

// Horizontal padding for the label, on both sides. Used with Material Design.
static const int kHorizontalLabelPaddingMD = 8;

// Solid shadow length from the label
static const int kVerticalShadowOffset = 1;

// Amount of blur applied to the label shadow
static const int kShadowBlur = 10;

// Height of an item header in Material Design.
static const int kHeaderHeight = 32;

// Opacity for dimmed items.
static const float kDimmedItemOpacity = 0.5f;

// Opacity for fading out during closing a window.
static const float kClosingItemOpacity = 0.8f;

// Duration of background opacity transition for the selected label.
static const int kSelectorFadeInMilliseconds = 350;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
static const float kPreCloseScale = 0.02f;

// Calculates the |window| bounds after being transformed to the selector's
// space. The returned Rect is in virtual screen coordinates.
gfx::Rect GetTransformedBounds(WmWindow* window) {
  gfx::RectF bounds(
      window->GetRootWindow()->ConvertRectToScreen(window->GetTargetBounds()));
  gfx::Transform new_transform = TransformAboutPivot(
      gfx::Point(bounds.x(), bounds.y()), window->GetTargetTransform());
  new_transform.TransformRect(&bounds);

  // With Material Design the preview title is shown above the preview window.
  // Hide the window header for apps or browser windows with no tabs (web apps)
  // to avoid showing both the window header and the preview title.
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    gfx::RectF header_bounds(bounds);
    header_bounds.set_height(
        window->GetIntProperty(WmWindowProperty::TOP_VIEW_INSET));
    new_transform.TransformRect(&header_bounds);
    bounds.Inset(0, gfx::ToCeiledInt(header_bounds.height()), 0, 0);
  }
  return ToEnclosingRect(bounds);
}

// Convenience method to fade in a Window with predefined animation settings.
// Note: The fade in animation will occur after a delay where the delay is how
// long the lay out animations take.
void SetupFadeInAfterLayout(views::Widget* widget) {
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget);
  window->SetOpacity(0.0f);
  std::unique_ptr<ScopedOverviewAnimationSettings>
      scoped_overview_animation_settings =
          ScopedOverviewAnimationSettingsFactory::Get()
              ->CreateOverviewAnimationSettings(
                  OverviewAnimationType::
                      OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
                  window);
  window->SetOpacity(1.0f);
}

// An image button with a close window icon.
class OverviewCloseButton : public views::ImageButton {
 public:
  explicit OverviewCloseButton(views::ButtonListener* listener);
  ~OverviewCloseButton() override;

 private:
  gfx::ImageSkia icon_image_;

  DISALLOW_COPY_AND_ASSIGN(OverviewCloseButton);
};

OverviewCloseButton::OverviewCloseButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    icon_image_ = gfx::CreateVectorIcon(gfx::VectorIconId::WINDOW_CONTROL_CLOSE,
                                        kCloseButtonColor);
    SetImage(views::CustomButton::STATE_NORMAL, &icon_image_);
    SetImage(views::CustomButton::STATE_HOVERED, &icon_image_);
    SetImage(views::CustomButton::STATE_PRESSED, &icon_image_);
    SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                      views::ImageButton::ALIGN_MIDDLE);
    SetMinimumImageSize(gfx::Size(kHeaderHeight, kHeaderHeight));
  } else {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    SetImage(views::CustomButton::STATE_NORMAL,
             rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE));
    SetImage(views::CustomButton::STATE_HOVERED,
             rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE_H));
    SetImage(views::CustomButton::STATE_PRESSED,
             rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE_P));
  }
}

OverviewCloseButton::~OverviewCloseButton() {}

// A View having rounded corners and a specified background color which is
// only painted within the bounds defined by the rounded corners.
// TODO(varkha): This duplicates code from RoundedImageView. Refactor these
//               classes and move into ui/views.
class RoundedContainerView : public views::View {
 public:
  RoundedContainerView(int corner_radius, SkColor background)
      : corner_radius_(corner_radius), background_(background) {}

  ~RoundedContainerView() override {}

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    SkScalar radius = SkIntToScalar(corner_radius_);
    const SkScalar kRadius[8] = {radius, radius, radius, radius,
                                 radius, radius, radius, radius};
    SkPath path;
    gfx::Rect bounds(size());
    bounds.set_height(bounds.height() + radius);
    path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);

    SkPaint paint;
    paint.setAntiAlias(true);
    canvas->ClipPath(path, true);
    canvas->DrawColor(background_);
  }

 private:
  int corner_radius_;
  SkColor background_;

  DISALLOW_COPY_AND_ASSIGN(RoundedContainerView);
};

}  // namespace

WindowSelectorItem::OverviewLabelButton::OverviewLabelButton(
    views::ButtonListener* listener,
    const base::string16& text)
    : LabelButton(listener, text) {}

WindowSelectorItem::OverviewLabelButton::~OverviewLabelButton() {}

void WindowSelectorItem::OverviewLabelButton::SetBackgroundColor(
    SkColor color) {
  label()->SetBackgroundColor(color);
}

gfx::Rect WindowSelectorItem::OverviewLabelButton::GetChildAreaBounds() {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(padding_);
  if (ash::MaterialDesignController::IsOverviewMaterial())
    bounds.Inset(kHorizontalLabelPaddingMD, 0, kHorizontalLabelPaddingMD, 0);
  return bounds;
}

// Container View that has an item label and a close button as children.
class WindowSelectorItem::CaptionContainerView : public views::View {
 public:
  CaptionContainerView(WindowSelectorItem::OverviewLabelButton* label,
                       views::ImageButton* close_button)
      : label_(label), close_button_(close_button) {
    AddChildView(label_);
    AddChildView(close_button_);
  }

 protected:
  // views::View:
  void Layout() override {
    // Position close button in the top right corner sized to its icon size and
    // the label in the top left corner as tall as the button and extending to
    // the button's left edge.
    // The rest of this container view serves as a shield to prevent input
    // events from reaching the transformed window in overview.
    gfx::Rect bounds(GetLocalBounds());
    bounds.Inset(kWindowSelectorMargin, kWindowSelectorMargin);
    const int visible_height = close_button_->GetPreferredSize().height();
    gfx::Insets label_padding(0, 0, bounds.height() - visible_height,
                              visible_height);
    label_->set_padding(label_padding);
    label_->SetBoundsRect(bounds);
    bounds.set_x(bounds.right() - visible_height);
    bounds.set_width(visible_height);
    bounds.set_height(visible_height);
    close_button_->SetBoundsRect(bounds);
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override { Layout(); }

 private:
  WindowSelectorItem::OverviewLabelButton* label_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(CaptionContainerView);
};

WindowSelectorItem::WindowSelectorItem(WmWindow* window,
                                       WindowSelector* window_selector)
    : dimmed_(false),
      root_window_(window->GetRootWindow()),
      transform_window_(window),
      in_bounds_update_(false),
      caption_container_view_(nullptr),
      window_label_button_view_(nullptr),
      close_button_(new OverviewCloseButton(this)),
      window_selector_(window_selector) {
  CreateWindowLabel(window->GetTitle());
  if (!ash::MaterialDesignController::IsOverviewMaterial()) {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_POPUP;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    close_button_widget_.reset(new views::Widget);
    close_button_widget_->set_focus_on_creation(false);
    window->GetRootWindowController()->ConfigureWidgetInitParamsForContainer(
        close_button_widget_.get(), kShellWindowId_OverlayContainer, &params);
    close_button_widget_->Init(params);
    close_button_->SetVisible(false);
    close_button_widget_->SetContentsView(close_button_);
    close_button_widget_->SetSize(close_button_->GetPreferredSize());
    close_button_widget_->Show();

    gfx::Rect close_button_rect(close_button_->GetPreferredSize());
    // Align the center of the button with position (0, 0) so that the
    // translate transform does not need to take the button dimensions into
    // account.
    close_button_rect.set_x(-close_button_rect.width() / 2);
    close_button_rect.set_y(-close_button_rect.height() / 2);
    WmLookup::Get()
        ->GetWindowForWidget(close_button_widget_.get())
        ->SetBounds(close_button_rect);
  }
  GetWindow()->AddObserver(this);
}

WindowSelectorItem::~WindowSelectorItem() {
  GetWindow()->RemoveObserver(this);
}

WmWindow* WindowSelectorItem::GetWindow() {
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

bool WindowSelectorItem::Contains(const WmWindow* target) const {
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

void WindowSelectorItem::SetSelected(bool selected) {
  if (!ash::MaterialDesignController::IsOverviewMaterial())
    return;
  WmWindow* window =
      WmLookup::Get()->GetWindowForWidget(window_label_selector_.get());
  ui::ScopedLayerAnimationSettings animation_settings(
      window->GetLayer()->GetAnimator());
  animation_settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kSelectorFadeInMilliseconds));
  animation_settings.SetTweenType(selected ? gfx::Tween::FAST_OUT_LINEAR_IN
                                           : gfx::Tween::LINEAR_OUT_SLOW_IN);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  window->SetOpacity(selected ? 0.0f : 1.0f);
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
  window_label_button_view_->NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION,
                                                      true);
}

void WindowSelectorItem::CloseWindow() {
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    gfx::Rect inset_bounds(target_bounds_);
    inset_bounds.Inset(target_bounds_.width() * kPreCloseScale,
                       target_bounds_.height() * kPreCloseScale);
    OverviewAnimationType animation_type =
        OverviewAnimationType::OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM;
    // Scale down both the window and label.
    SetBounds(inset_bounds, animation_type);
    // First animate opacity to an intermediate value concurrently with the
    // scaling animation.
    AnimateOpacity(kClosingItemOpacity, animation_type);

    // Fade out the window and the label, effectively hiding them.
    AnimateOpacity(
        0.0, OverviewAnimationType::OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM);
  }
  transform_window_.Close();
}

void WindowSelectorItem::SetDimmed(bool dimmed) {
  dimmed_ = dimmed;
  SetOpacity(dimmed ? kDimmedItemOpacity : 1.0f);
}

void WindowSelectorItem::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == close_button_) {
    WmShell::Get()->RecordUserMetricsAction(UMA_WINDOW_OVERVIEW_CLOSE_BUTTON);
    CloseWindow();
    return;
  }
  CHECK(sender == window_label_button_view_);
  window_selector_->SelectWindow(transform_window_.window());
}

void WindowSelectorItem::OnWindowDestroying(WmWindow* window) {
  window->RemoveObserver(this);
  transform_window_.OnWindowDestroyed();
}

void WindowSelectorItem::OnWindowTitleChanged(WmWindow* window) {
  // TODO(flackr): Maybe add the new title to a vector of titles so that we can
  // filter any of the titles the window had while in the overview session.
  window_label_button_view_->SetText(window->GetTitle());
  UpdateCloseButtonAccessibilityName();
}

float WindowSelectorItem::GetItemScale(const gfx::Size& size) {
  gfx::Size inset_size(size.width(), size.height() - 2 * kWindowMarginMD);
  return ScopedTransformOverviewWindow::GetItemScale(
      GetWindow()->GetTargetBounds().size(), inset_size,
      GetWindow()->GetIntProperty(WmWindowProperty::TOP_VIEW_INSET),
      close_button_->GetPreferredSize().height());
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
    top_view_inset =
        GetWindow()->GetIntProperty(WmWindowProperty::TOP_VIEW_INSET);
    title_height = close_button_->GetPreferredSize().height();
  }
  gfx::Rect selector_item_bounds =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          screen_rect, target_bounds, top_view_inset, title_height);
  gfx::Transform transform = ScopedTransformOverviewWindow::GetTransformForRect(
      screen_rect, selector_item_bounds);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  // Rounded corners are achieved by using a mask layer on the original window
  // before the transform. Dividing by scale factor obtains the corner radius
  // which when scaled will yield |kLabelBackgroundRadius|.
  transform_window_.SetTransform(
      root_window_, transform,
      (kLabelBackgroundRadius / GetItemScale(target_bounds.size())));
  transform_window_.set_overview_transform(transform);
}

void WindowSelectorItem::SetOpacity(float opacity) {
  window_label_->SetOpacity(opacity);
  if (!ash::MaterialDesignController::IsOverviewMaterial())
    close_button_widget_->SetOpacity(opacity);

  transform_window_.SetOpacity(opacity);
}

void WindowSelectorItem::UpdateWindowLabel(
    const gfx::Rect& window_bounds,
    OverviewAnimationType animation_type) {
  if (!window_label_->IsVisible()) {
    window_label_->Show();
    SetupFadeInAfterLayout(window_label_.get());
  }

  gfx::Rect label_bounds = root_window_->ConvertRectFromScreen(window_bounds);
  window_label_button_view_->set_padding(
      gfx::Insets(label_bounds.height() - kVerticalLabelPadding, 0, 0, 0));
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings =
      ScopedOverviewAnimationSettingsFactory::Get()
          ->CreateOverviewAnimationSettings(
              animation_type,
              WmLookup::Get()->GetWindowForWidget(window_label_.get()));

  WmWindow* window_label_window =
      WmLookup::Get()->GetWindowForWidget(window_label_.get());
  window_label_window->SetBounds(label_bounds);
}

void WindowSelectorItem::CreateWindowLabel(const base::string16& title) {
  const bool material = ash::MaterialDesignController::IsOverviewMaterial();
  window_label_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.visible_on_all_workspaces = true;
  window_label_->set_focus_on_creation(false);
  root_window_->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          window_label_.get(), kShellWindowId_OverlayContainer, &params);
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
  window_label_button_view_->SetFontList(bundle.GetFontList(
      material ? ui::ResourceBundle::BaseFont : ui::ResourceBundle::BoldFont));
  if (material) {
    caption_container_view_ =
        new CaptionContainerView(window_label_button_view_, close_button_);
    window_label_->SetContentsView(caption_container_view_);
    window_label_button_view_->SetVisible(false);
    window_label_->Show();

    views::View* background_view =
        new RoundedContainerView(kLabelBackgroundRadius, kLabelBackgroundColor);
    window_label_selector_.reset(new views::Widget);
    params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
    params.accept_events = false;
    window_label_selector_->Init(params);
    window_label_selector_->set_focus_on_creation(false);
    window_label_selector_->SetContentsView(background_view);
    window_label_selector_->Show();
  } else {
    window_label_->SetContentsView(window_label_button_view_);
  }
}

void WindowSelectorItem::UpdateHeaderLayout(
    OverviewAnimationType animation_type) {
  gfx::Rect transformed_window_bounds =
      root_window_->ConvertRectFromScreen(GetTransformedBounds(GetWindow()));

  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    gfx::Rect label_rect(close_button_->GetPreferredSize());
    label_rect.set_y(-label_rect.height());
    label_rect.set_width(transformed_window_bounds.width());

    if (!window_label_button_view_->visible()) {
      window_label_button_view_->SetVisible(true);
      SetupFadeInAfterLayout(window_label_.get());
      SetupFadeInAfterLayout(window_label_selector_.get());
    }
    WmWindow* window_label_window =
        WmLookup::Get()->GetWindowForWidget(window_label_.get());
    WmWindow* window_label_selector_window =
        WmLookup::Get()->GetWindowForWidget(window_label_selector_.get());
    std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings =
        ScopedOverviewAnimationSettingsFactory::Get()
            ->CreateOverviewAnimationSettings(animation_type,
                                              window_label_window);
    std::unique_ptr<ScopedOverviewAnimationSettings>
        animation_settings_selector =
            ScopedOverviewAnimationSettingsFactory::Get()
                ->CreateOverviewAnimationSettings(animation_type,
                                                  window_label_selector_window);
    window_label_selector_window->SetBounds(label_rect);
    // |window_label_window| covers both the transformed window and the header
    // as well as the gap between the windows to prevent events from reaching
    // the window including its sizing borders.
    label_rect.set_height(label_rect.height() +
                          transformed_window_bounds.height());
    label_rect.Inset(-kWindowSelectorMargin, -kWindowSelectorMargin);
    window_label_window->SetBounds(label_rect);
    gfx::Transform label_transform;
    label_transform.Translate(transformed_window_bounds.x(),
                              transformed_window_bounds.y());
    window_label_window->SetTransform(label_transform);
    window_label_selector_window->SetTransform(label_transform);
  } else {
    if (!close_button_->visible()) {
      close_button_->SetVisible(true);
      SetupFadeInAfterLayout(close_button_widget_.get());
    }
    WmWindow* close_button_widget_window =
        WmLookup::Get()->GetWindowForWidget(close_button_widget_.get());
    std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings =
        ScopedOverviewAnimationSettingsFactory::Get()
            ->CreateOverviewAnimationSettings(animation_type,
                                              close_button_widget_window);

    gfx::Transform close_button_transform;
    close_button_transform.Translate(transformed_window_bounds.right(),
                                     transformed_window_bounds.y());
    close_button_widget_window->SetTransform(close_button_transform);
  }
}

void WindowSelectorItem::AnimateOpacity(float opacity,
                                        OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  WmWindow* window_label_window =
      WmLookup::Get()->GetWindowForWidget(window_label_.get());
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings_label =
      ScopedOverviewAnimationSettingsFactory::Get()
          ->CreateOverviewAnimationSettings(animation_type,
                                            window_label_window);
  window_label_window->SetOpacity(opacity);

  WmWindow* window_label_selector_window =
      WmLookup::Get()->GetWindowForWidget(window_label_selector_.get());
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings_selector =
      ScopedOverviewAnimationSettingsFactory::Get()
          ->CreateOverviewAnimationSettings(animation_type,
                                            window_label_selector_window);
  window_label_selector_window->SetOpacity(opacity);
}

void WindowSelectorItem::UpdateCloseButtonAccessibilityName() {
  close_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_OVERVIEW_CLOSE_ITEM_BUTTON_ACCESSIBLE_NAME,
      GetWindow()->GetTitle()));
}

}  // namespace ash
