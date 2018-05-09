// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/events/event.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

void BubbleIconView::Init() {
  AddChildView(image_);
  image_->set_can_process_events_within_subtree(false);
  image_->EnableCanvasFlippingForRTLUI(true);
  SetInkDropMode(InkDropMode::ON);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

BubbleIconView::BubbleIconView(CommandUpdater* command_updater,
                               int command_id,
                               BubbleIconView::Delegate* delegate)
    : widget_observer_(this),
      image_(new views::ImageView()),
      command_updater_(command_updater),
      delegate_(delegate),
      command_id_(command_id),
      active_(false),
      suppress_mouse_released_action_(false) {
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(GetLayoutConstant(LOCATION_BAR_ICON_INTERIOR_PADDING))));
  if (ui::MaterialDesignController::IsNewerMaterialUi()) {
    // Ink drop ripple opacity.
    set_ink_drop_visible_opacity(
        GetOmniboxStateAlpha(OmniboxPartState::SELECTED));
  }
}

BubbleIconView::~BubbleIconView() {}

bool BubbleIconView::IsBubbleShowing() const {
  // If the bubble is being destroyed, it's considered showing though it may be
  // already invisible currently.
  return GetBubble() != nullptr;
}

bool BubbleIconView::SetCommandEnabled(bool enabled) const {
  DCHECK(command_updater_);
  command_updater_->UpdateCommandEnabled(command_id_, enabled);
  return command_updater_->IsCommandEnabled(command_id_);
}

void BubbleIconView::SetImage(const gfx::ImageSkia* image_skia) {
  image_->SetImage(image_skia);
}

const gfx::ImageSkia& BubbleIconView::GetImage() const {
  return image_->GetImage();
}

void BubbleIconView::SetHighlighted(bool bubble_visible) {
  AnimateInkDrop(bubble_visible ? views::InkDropState::ACTIVATED
                                : views::InkDropState::DEACTIVATED,
                 nullptr);
}

void BubbleIconView::OnBubbleWidgetCreated(views::Widget* bubble_widget) {
  widget_observer_.SetWidget(bubble_widget);

  if (bubble_widget->IsVisible())
    SetHighlighted(true);
}

bool BubbleIconView::Refresh() {
  return false;
}

void BubbleIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(GetTextForTooltipAndAccessibleName());
}

bool BubbleIconView::GetTooltipText(const gfx::Point& p,
                                    base::string16* tooltip) const {
  if (IsBubbleShowing())
    return false;
  *tooltip = GetTextForTooltipAndAccessibleName();
  return true;
}

gfx::Size BubbleIconView::CalculatePreferredSize() const {
  gfx::Size image_rect(image_->GetPreferredSize());
  image_rect.Enlarge(GetInsets().width(), GetInsets().height());
  return image_rect;
}

void BubbleIconView::Layout() {
  image_->SetBoundsRect(GetContentsBounds());
}

bool BubbleIconView::OnMousePressed(const ui::MouseEvent& event) {
  // If the bubble is showing then don't reshow it when the mouse is released.
  suppress_mouse_released_action_ = IsBubbleShowing();
  if (!suppress_mouse_released_action_ && event.IsOnlyLeftMouseButton())
    AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);

  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void BubbleIconView::OnMouseReleased(const ui::MouseEvent& event) {
  // If this is the second click on this view then the bubble was showing on the
  // mouse pressed event and is hidden now. Prevent the bubble from reshowing by
  // doing nothing here.
  if (suppress_mouse_released_action_) {
    suppress_mouse_released_action_ = false;
    OnPressed(false);
    return;
  }
  if (!event.IsLeftMouseButton())
    return;

  const bool activated = HitTestPoint(event.location());
  AnimateInkDrop(
      activated ? views::InkDropState::ACTIVATED : views::InkDropState::HIDDEN,
      &event);
  if (activated)
    ExecuteCommand(EXECUTE_SOURCE_MOUSE);
  OnPressed(activated);
}

bool BubbleIconView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_RETURN && event.key_code() != ui::VKEY_SPACE)
    return false;

  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr /* &event */);
  // As with Button, return activates on key down and space activates on
  // key up.
  if (event.key_code() == ui::VKEY_RETURN)
    ExecuteCommand(EXECUTE_SOURCE_KEYBOARD);
  return true;
}

bool BubbleIconView::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_SPACE)
    return false;

  ExecuteCommand(EXECUTE_SOURCE_KEYBOARD);
  return true;
}

void BubbleIconView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && GetNativeTheme())
    UpdateIcon();
}

void BubbleIconView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateIcon();
}

void BubbleIconView::OnThemeChanged() {
  UpdateIcon();
}

void BubbleIconView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  image_->SetPaintToLayer();
  image_->layer()->SetFillsBoundsOpaquely(false);
  views::InkDropHostView::AddInkDropLayer(ink_drop_layer);
}

void BubbleIconView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  views::InkDropHostView::RemoveInkDropLayer(ink_drop_layer);
  image_->DestroyLayer();
}

std::unique_ptr<views::InkDrop> BubbleIconView::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnFocus(true);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> BubbleIconView::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
BubbleIconView::CreateInkDropHighlight() const {
  std::unique_ptr<views::InkDropHighlight> highlight =
      CreateDefaultInkDropHighlight(
          gfx::RectF(GetMirroredRect(GetContentsBounds())).CenterPoint(),
          size());
  if (ui::MaterialDesignController::IsNewerMaterialUi()) {
    highlight->set_visible_opacity(
        GetOmniboxStateAlpha(OmniboxPartState::HOVERED));
  }
  return highlight;
}

SkColor BubbleIconView::GetInkDropBaseColor() const {
  const SkColor ink_color_opaque = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
  if (ui::MaterialDesignController::IsNewerMaterialUi()) {
    // Opacity of the ink drop is set elsewhere, so just use full opacity here.
    return ink_color_opaque;
  }
  return color_utils::DeriveDefaultIconColor(ink_color_opaque);
}

std::unique_ptr<views::InkDropMask> BubbleIconView::CreateInkDropMask() const {
  if (!LocationBarView::IsRounded())
    return nullptr;
  return std::make_unique<views::RoundRectInkDropMask>(size(), gfx::Insets(),
                                                       height() / 2.f);
}

void BubbleIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    AnimateInkDrop(views::InkDropState::ACTIVATED, event);
    ExecuteCommand(EXECUTE_SOURCE_GESTURE);
    event->SetHandled();
  }
}

void BubbleIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  if (command_updater_)
    command_updater_->ExecuteCommand(command_id_);
}

void BubbleIconView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::BubbleDialogDelegateView* bubble = GetBubble();
  if (bubble)
    bubble->OnAnchorBoundsChanged();
  InkDropHostView::OnBoundsChanged(previous_bounds);
}

void BubbleIconView::UpdateIcon() {
  const ui::NativeTheme* theme = GetNativeTheme();
  SkColor icon_color =
      active_ ? theme->GetSystemColor(
                    ui::NativeTheme::kColorId_ProminentButtonColor)
              : GetOmniboxColor(OmniboxPart::LOCATION_BAR_SECURITY_CHIP,
                                delegate_->GetTint(),
                                OmniboxPartState::CHIP_DEFAULT);
  image_->SetImage(gfx::CreateVectorIcon(
      GetVectorIcon(), GetLayoutConstant(LOCATION_BAR_ICON_SIZE), icon_color));
}

void BubbleIconView::SetActiveInternal(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  UpdateIcon();
}

content::WebContents* BubbleIconView::GetWebContents() const {
  return delegate_->GetWebContentsForBubbleIconView();
}

BubbleIconView::WidgetObserver::WidgetObserver(BubbleIconView* parent)
    : parent_(parent), scoped_observer_(this) {}

BubbleIconView::WidgetObserver::~WidgetObserver() = default;

void BubbleIconView::WidgetObserver::SetWidget(views::Widget* widget) {
  scoped_observer_.RemoveAll();
  scoped_observer_.Add(widget);
}

void BubbleIconView::WidgetObserver::OnWidgetDestroying(views::Widget* widget) {
  scoped_observer_.Remove(widget);
}

void BubbleIconView::WidgetObserver::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  // |widget| is a bubble that has just got shown / hidden.
  parent_->SetHighlighted(visible);
}
