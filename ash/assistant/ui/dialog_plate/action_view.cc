// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/action_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredSizeDip = 24;

// TODO(dmblack): Remove after LogoView is implemented.
// Temporary colors to represent states.
constexpr SkColor kKeyboardColor = SK_ColorTRANSPARENT;
constexpr SkColor kMicOpenColor = SkColorSetRGB(0xDD, 0x51, 0x44);     // Red
constexpr SkColor kMicClosedColor = SkColorSetA(SK_ColorBLACK, 0x1F);  // Grey

}  // namespace

ActionView::ActionView(AssistantController* assistant_controller,
                       ActionViewListener* listener)
    : assistant_controller_(assistant_controller),
      listener_(listener),
      keyboard_action_view_(new views::ImageView()) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // ActionView belongs so is guaranteed to outlive it.
  assistant_controller_->AddInteractionModelObserver(this);

  // Set initial state.
  UpdateState();
}

ActionView::~ActionView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

gfx::Size ActionView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredSizeDip, kPreferredSizeDip);
}

void ActionView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Keyboard action.
  keyboard_action_view_->SetImage(
      gfx::CreateVectorIcon(kSendIcon, kPreferredSizeDip, gfx::kGoogleBlue500));
  keyboard_action_view_->SetImageSize(
      gfx::Size(kPreferredSizeDip, kPreferredSizeDip));
  keyboard_action_view_->SetPreferredSize(
      gfx::Size(kPreferredSizeDip, kPreferredSizeDip));
  AddChildView(keyboard_action_view_);

  // TODO(dmblack): Add once LogoView has been implemented.
  // Voice action.
}

// TODO(dmblack): Remove after LogoView is implemented.
void ActionView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color_);
  canvas->DrawRoundRect(GetContentsBounds(), kPreferredSizeDip / 2, flags);
}

void ActionView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;

  event->SetHandled();

  if (listener_)
    listener_->OnActionPressed();
}

bool ActionView::OnMousePressed(const ui::MouseEvent& event) {
  if (listener_)
    listener_->OnActionPressed();
  return true;
}

void ActionView::OnInputModalityChanged(InputModality input_modality) {
  UpdateState();
}

void ActionView::OnMicStateChanged(MicState mic_state) {
  UpdateState();
}

// TODO(dmblack): Currently state is being indicated by painting a different
// background color. This will be replaced once the stateful LogoView has been
// implemented (b/78191547).
void ActionView::UpdateState() {
  const AssistantInteractionModel* interaction_model =
      assistant_controller_->interaction_model();

  InputModality input_modality = interaction_model->input_modality();

  // We don't need to handle stylus input modality.
  if (input_modality == InputModality::kStylus)
    return;

  if (input_modality == InputModality::kKeyboard) {
    keyboard_action_view_->SetVisible(true);
    color_ = kKeyboardColor;
    SchedulePaint();
    return;
  }

  // TODO(dmblack): Make voice action view visible once it has been added.
  keyboard_action_view_->SetVisible(false);

  switch (interaction_model->mic_state()) {
    case MicState::kClosed:
      color_ = kMicClosedColor;
      break;
    case MicState::kOpen:
      color_ = kMicOpenColor;
      break;
  }

  SchedulePaint();
}

}  // namespace ash
