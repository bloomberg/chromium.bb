// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view.h"

#include <algorithm>

#include "ash/public/cpp/ash_features.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDeskPreviewHeight = 64;

constexpr int kLabelPreviewSpacing = 8;

constexpr int kCloseButtonMargin = 2;

constexpr gfx::Size kCloseButtonSize{24, 24};

constexpr int kPreviewCornerRadius = 2;

constexpr SkColor kActiveColor = SkColorSetARGB(0xEE, 0xFF, 0xFF, 0xFF);

constexpr SkColor kInactiveColor = SkColorSetARGB(0x50, 0xFF, 0xFF, 0xFF);

// The desk preview bounds are proportional to the bounds of the display on
// which it resides, but always has a fixed height `kDeskPreviewHeight`.
gfx::Rect GetDeskPreviewBounds(aura::Window* root_window) {
  const auto root_size = root_window->GetBoundsInRootWindow().size();
  return gfx::Rect(kDeskPreviewHeight * root_size.width() / root_size.height(),
                   kDeskPreviewHeight);
}

}  // namespace

// -----------------------------------------------------------------------------

// A view that shows the contents of the corresponding desk in its mini_view.
class DeskPreviewView : public views::View {
 public:
  explicit DeskPreviewView(DeskMiniView* mini_view) : mini_view_(mini_view) {
    // For now use a solid color layer.
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    if (features::ShouldUseShaderRoundedCorner()) {
      layer()->SetRoundedCornerRadius(
          {kPreviewCornerRadius, kPreviewCornerRadius, kPreviewCornerRadius,
           kPreviewCornerRadius});
    }

    // TODO(afakhry): Mirror the contents of the corresponding desk.
  }

  ~DeskPreviewView() override = default;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    switch (event->type()) {
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_MOUSE_DRAGGED:
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_ENTERED:
      case ui::ET_MOUSE_EXITED:
        mini_view_->OnHoverStateMayHaveChanged();
        break;

      default:
        break;
    }
    views::View::OnMouseEvent(event);
  }

 private:
  DeskMiniView* mini_view_;

  DISALLOW_COPY_AND_ASSIGN(DeskPreviewView);
};

// -----------------------------------------------------------------------------
// DeskMiniView

DeskMiniView::DeskMiniView(const Desk* desk,
                           const base::string16& title,
                           views::ButtonListener* listener)
    : views::Button(listener),
      desk_(desk),
      desk_preview_(new DeskPreviewView(this)),
      label_(new views::Label(title)),
      close_desk_button_(new CloseDeskButton(this)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  label_->set_can_process_events_within_subtree(false);
  label_->SetEnabledColor(SK_ColorWHITE);
  label_->SetLineHeight(10);

  close_desk_button_->SetVisible(false);

  // TODO(afakhry): Tooltips and accessible names.

  AddChildView(desk_preview_);
  AddChildView(label_);
  AddChildView(close_desk_button_);

  SetFocusPainter(nullptr);
  SetInkDropMode(InkDropMode::OFF);

  UpdateActivationState();

  SchedulePaint();
}

DeskMiniView::~DeskMiniView() = default;

void DeskMiniView::SetTitle(const base::string16& title) {
  label_->SetText(title);
}

void DeskMiniView::OnHoverStateMayHaveChanged() {
  // TODO(afakhry): In tablet mode, discuss showing the close button on long
  // press. Also, don't show the close button when hovered while window drag is
  // in progress.
  close_desk_button_->SetVisible(DesksController::Get()->CanRemoveDesks() &&
                                 IsMouseHovered());
}

void DeskMiniView::UpdateActivationState() {
  desk_preview_->layer()->SetColor(desk_->is_active() ? kActiveColor
                                                      : kInactiveColor);
}

const char* DeskMiniView::GetClassName() const {
  return "DeskMiniView";
}

void DeskMiniView::Layout() {
  auto* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);

  const gfx::Rect preview_bounds = GetDeskPreviewBounds(root_window);
  desk_preview_->SetBoundsRect(preview_bounds);

  const gfx::Size label_size = label_->GetPreferredSize();
  const gfx::Rect label_bounds{
      (preview_bounds.width() - label_size.width()) / 2,
      preview_bounds.bottom() + kLabelPreviewSpacing, label_size.width(),
      label_size.height()};
  label_->SetBoundsRect(label_bounds);

  close_desk_button_->SetBounds(
      preview_bounds.right() - kCloseButtonSize.width() - kCloseButtonMargin,
      kCloseButtonMargin, kCloseButtonSize.width(), kCloseButtonSize.height());

  Button::Layout();

  SchedulePaint();
}

gfx::Size DeskMiniView::CalculatePreferredSize() const {
  auto* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);

  const gfx::Size label_size = label_->GetPreferredSize();
  const gfx::Rect preview_bounds = GetDeskPreviewBounds(root_window);

  return gfx::Size{
      std::max(preview_bounds.width(), label_size.width()),
      preview_bounds.height() + kLabelPreviewSpacing + label_size.height()};
}

void DeskMiniView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  if (sender != close_desk_button_)
    return;

  // Hide the close button so it can no longer be pressed.
  close_desk_button_->SetVisible(false);

  // This mini_view can no longer be pressed.
  listener_ = nullptr;

  auto* controller = DesksController::Get();
  DCHECK(controller->CanRemoveDesks());
  controller->RemoveDesk(desk_);
}

void DeskMiniView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      OnHoverStateMayHaveChanged();
      break;

    default:
      break;
  }
  views::Button::OnMouseEvent(event);
}

}  // namespace ash
