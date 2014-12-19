// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_item.h"

#include <algorithm>
#include <vector>

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
static const int kWindowMargin = 30;

// Foreground label color.
static const SkColor kLabelColor = SK_ColorWHITE;

// Background label color.
static const SkColor kLabelBackground = SK_ColorTRANSPARENT;

// Label shadow color.
static const SkColor kLabelShadow = 0xB0000000;

// Vertical padding for the label, both over and beneath it.
static const int kVerticalLabelPadding = 20;

// Solid shadow length from the label
static const int kVerticalShadowOffset = 1;

// Amount of blur applied to the label shadow
static const int kShadowBlur = 10;

// Opacity for dimmed items.
static const float kDimmedItemOpacity = 0.5f;

// Calculates the |window| bounds after being transformed to the selector's
// space. The returned Rect is in virtual screen coordinates.
gfx::Rect GetTransformedBounds(aura::Window* window) {
  gfx::RectF bounds(ScreenUtil::ConvertRectToScreen(window->GetRootWindow(),
      window->layer()->GetTargetBounds()));
  gfx::Transform new_transform = TransformAboutPivot(
      gfx::Point(bounds.x(), bounds.y()),
      window->layer()->GetTargetTransform());
  new_transform.TransformRect(&bounds);
  return ToEnclosingRect(bounds);
}

// Convenvience method to fade in a Window with predefined animation settings.
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

WindowSelectorItem::WindowSelectorItem(aura::Window* root_window)
    : dimmed_(false),
      root_window_(root_window),
      in_bounds_update_(false),
      window_label_view_(nullptr),
      close_button_(new OverviewCloseButton(this)),
      selector_item_activate_window_button_(
          new TransparentActivateWindowButton(root_window, this)) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = Shell::GetContainer(root_window,
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
  close_button_rect.set_x(-close_button_rect.width() / 2);
  close_button_rect.set_y(-close_button_rect.height() / 2);
  close_button_widget_.GetNativeWindow()->SetBounds(close_button_rect);
}

WindowSelectorItem::~WindowSelectorItem() {
  for (auto* transform_window : transform_windows_) {
    transform_window->window()->RemoveObserver(this);
  }
}

void WindowSelectorItem::AddWindow(aura::Window* window) {
  DCHECK(window->GetRootWindow() == root_window_);
  window->AddObserver(this);
  ScopedTransformOverviewWindow* transform_window =
      new ScopedTransformOverviewWindow(window);
  transform_windows_.push_back(transform_window);
  // The transparent overlays are added at the front of the z-order when
  // created so make sure the selector item's transparent overlay is behind the
  // overlay for the window that was just added.
  transform_window->activate_button()->StackAbove(
      selector_item_activate_window_button_.get());

  UpdateSelectorButtons();
  UpdateCloseButtonAccessibilityName();
}

bool WindowSelectorItem::HasSelectableWindow(const aura::Window* window) const {
  for (auto* transform_window : transform_windows_) {
    if (transform_window->window() == window)
      return true;
  }
  return false;
}

bool WindowSelectorItem::Contains(const aura::Window* target) const {
  for (auto* transform_window : transform_windows_) {
    if (transform_window->Contains(target))
      return true;
  }
  return false;
}

void WindowSelectorItem::RestoreWindowOnExit(aura::Window* window) {
  for (auto* transform_window : transform_windows_) {
    if (transform_window->Contains(window)) {
      transform_window->RestoreWindowOnExit();
      break;
    }
  }
}

aura::Window* WindowSelectorItem::SelectionWindow() const {
  return SelectionTransformWindow()->window();
}

void WindowSelectorItem::RemoveWindow(const aura::Window* window) {
  bool window_found = false;

  for (TransformWindows::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end();
       ++iter) {
    ScopedTransformOverviewWindow* transform_window = *iter;

    if (transform_window->window() == window) {
      transform_window->window()->RemoveObserver(this);
      transform_window->OnWindowDestroyed();
      transform_windows_.erase(iter);
      window_found = true;
      break;
    }
  }
  CHECK(window_found);


  // If empty WindowSelectorItem will be destroyed immediately after this by
  // its owner.
  if (empty())
    return;

  UpdateCloseButtonAccessibilityName();
  window_label_.reset();
  UpdateWindowLabels(target_bounds_,
                     OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  UpdateCloseButtonLayout(OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  UpdateSelectorButtons();
}

bool WindowSelectorItem::empty() const {
  return transform_windows_.empty();
}

void WindowSelectorItem::PrepareForOverview() {
  for (auto* transform_window : transform_windows_)
    transform_window->PrepareForOverview();
}

void WindowSelectorItem::SetBounds(aura::Window* root_window,
                                   const gfx::Rect& target_bounds,
                                   OverviewAnimationType animation_type) {
  if (in_bounds_update_)
    return;
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  target_bounds_ = target_bounds;

  UpdateWindowLabels(target_bounds, animation_type);

  gfx::Rect inset_bounds(target_bounds);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);
  SetItemBounds(root_window, inset_bounds, animation_type);

  // SetItemBounds is called before UpdateCloseButtonLayout so the close button
  // can properly use the updated windows bounds.
  UpdateCloseButtonLayout(animation_type);
  UpdateSelectorButtons();
}

void WindowSelectorItem::RecomputeWindowTransforms() {
  if (in_bounds_update_ || target_bounds_.IsEmpty())
    return;
  DCHECK(root_window_);
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  gfx::Rect inset_bounds(target_bounds_);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);
  SetItemBounds(root_window_, inset_bounds,
      OverviewAnimationType::OVERVIEW_ANIMATION_NONE);

  UpdateCloseButtonLayout(OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  UpdateSelectorButtons();
}

void WindowSelectorItem::SendFocusAlert() const {
  selector_item_activate_window_button_->SendFocusAlert();
}

void WindowSelectorItem::SetDimmed(bool dimmed) {
  dimmed_ = dimmed;
  SetOpacity(dimmed ? kDimmedItemOpacity : 1.0f);
}

void WindowSelectorItem::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  CHECK(!transform_windows_.empty());
  SelectionTransformWindow()->Close();
}

void WindowSelectorItem::OnWindowTitleChanged(aura::Window* window) {
  // TODO(flackr): Maybe add the new title to a vector of titles so that we can
  // filter any of the titles the window had while in the overview session.
  if (window == SelectionWindow()) {
    window_label_view_->SetText(window->title());
    UpdateCloseButtonAccessibilityName();
  }
  UpdateCloseButtonLayout(OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  UpdateSelectorButtons();
}

void WindowSelectorItem::Select() {
  aura::Window* selection_window = SelectionWindow();
  if (selection_window)
    wm::GetWindowState(selection_window)->Activate();
}

void WindowSelectorItem::SetItemBounds(aura::Window* root_window,
                                       const gfx::Rect& target_bounds,
                                       OverviewAnimationType animation_type) {
  gfx::Rect bounding_rect;
  for (auto* transform_window : transform_windows_) {
    bounding_rect.Union(
        transform_window->GetTargetBoundsInScreen());
  }
  gfx::Rect bounds =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          bounding_rect, target_bounds);
  gfx::Transform bounding_transform =
      ScopedTransformOverviewWindow::GetTransformForRect(bounding_rect, bounds);
  for (auto* transform_window : transform_windows_) {
    gfx::Rect target_bounds = transform_window->GetTargetBoundsInScreen();
    gfx::Transform transform = TransformAboutPivot(
        gfx::Point(bounding_rect.x() - target_bounds.x(),
                   bounding_rect.y() - target_bounds.y()),
        bounding_transform);

    ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
    transform_window->BeginScopedAnimation(animation_type, &animation_settings);
    transform_window->SetTransform(root_window, transform);
    transform_window->set_overview_transform(transform);
  }
}

void WindowSelectorItem::SetOpacity(float opacity) {
  window_label_->GetNativeWindow()->layer()->SetOpacity(opacity);
  close_button_widget_.GetNativeWindow()->layer()->SetOpacity(opacity);

  // TODO(flackr): find a way to make panels that are hidden behind other panels
  // look nice.
  for (auto* transform_window : transform_windows_) {
    transform_window->SetOpacity(opacity);
  }
}

void WindowSelectorItem::UpdateWindowLabels(
    const gfx::Rect& window_bounds,
    OverviewAnimationType animation_type) {
  // If the root window has changed, force the window label to be recreated
  // and faded in on the new root window.
  DCHECK(!window_label_ ||
         window_label_->GetNativeWindow()->GetRootWindow() == root_window_);

  if (!window_label_) {
    CreateWindowLabel(SelectionWindow()->title());
    SetupFadeInAfterLayout(window_label_->GetNativeWindow());
  }

  gfx::Rect converted_bounds = ScreenUtil::ConvertRectFromScreen(root_window_,
                                                                 window_bounds);
  gfx::Rect label_bounds(converted_bounds.x(),
                         converted_bounds.bottom(),
                         converted_bounds.width(),
                         0);
  label_bounds.set_height(window_label_->GetContentsView()->
                              GetPreferredSize().height());
  label_bounds.set_y(label_bounds.y() - window_label_->
                         GetContentsView()->GetPreferredSize().height());

  ScopedOverviewAnimationSettings animation_settings(animation_type,
      window_label_->GetNativeWindow());

  window_label_->GetNativeWindow()->SetBounds(label_bounds);
}

void WindowSelectorItem::CreateWindowLabel(const base::string16& title) {
  window_label_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = Shell::GetContainer(root_window_,
                                      kShellWindowId_OverlayContainer);
  params.accept_events = false;
  params.visible_on_all_workspaces = true;
  window_label_->set_focus_on_creation(false);
  window_label_->Init(params);
  window_label_view_ = new views::Label;
  window_label_view_->SetEnabledColor(kLabelColor);
  window_label_view_->SetBackgroundColor(kLabelBackground);
  window_label_view_->SetShadows(gfx::ShadowValues(
      1,
      gfx::ShadowValue(
          gfx::Point(0, kVerticalShadowOffset), kShadowBlur, kLabelShadow)));
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  window_label_view_->SetFontList(
      bundle.GetFontList(ui::ResourceBundle::BoldFont));
  window_label_view_->SetText(title);
  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  0,
                                                  kVerticalLabelPadding,
                                                  0);
  window_label_view_->SetLayoutManager(layout);
  window_label_->SetContentsView(window_label_view_);
  window_label_->Show();
}

void WindowSelectorItem::UpdateSelectorButtons() {
  CHECK(!transform_windows_.empty());

  selector_item_activate_window_button_->SetBounds(target_bounds());
  selector_item_activate_window_button_->SetAccessibleName(
      transform_windows_.front()->window()->title());

  for (auto* transform_window : transform_windows_) {
    TransparentActivateWindowButton* activate_button =
        transform_window->activate_button();

    // If there is only one window in this, then expand the transparent overlay
    // so that touch exploration in ChromVox only provides spoken feedback once
    // within |this| selector item's bounds.
    gfx::Rect bounds = transform_windows_.size() == 1
        ? target_bounds() : GetTransformedBounds(transform_window->window());
    activate_button->SetBounds(bounds);
    activate_button->SetAccessibleName(transform_window->window()->title());
  }
}

void WindowSelectorItem::UpdateCloseButtonLayout(
    OverviewAnimationType animation_type) {
  if (!close_button_->visible()) {
    close_button_->SetVisible(true);
    SetupFadeInAfterLayout(close_button_widget_.GetNativeWindow());
  }
  ScopedOverviewAnimationSettings animation_settings(animation_type,
      close_button_widget_.GetNativeWindow());

  gfx::Rect transformed_window_bounds = ScreenUtil::ConvertRectFromScreen(
      close_button_widget_.GetNativeWindow()->GetRootWindow(),
      GetTransformedBounds(SelectionWindow()));

  gfx::Transform close_button_transform;
  close_button_transform.Translate(transformed_window_bounds.right(),
                                   transformed_window_bounds.y());
  close_button_widget_.GetNativeWindow()->SetTransform(
      close_button_transform);
}

void WindowSelectorItem::UpdateCloseButtonAccessibilityName() {
  close_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_OVERVIEW_CLOSE_ITEM_BUTTON_ACCESSIBLE_NAME,
      SelectionWindow()->title()));
}

ScopedTransformOverviewWindow*
    WindowSelectorItem::SelectionTransformWindow() const {
  CHECK(!transform_windows_.empty());
  return transform_windows_.front();
}

}  // namespace ash
