// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/metalayer_mode.h"

#include "ash/palette_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

namespace {

const char kToastId[] = "palette_metalayer_mode";
const int kToastDurationMs = 2500;

}  // namespace

MetalayerMode::MetalayerMode(Delegate* delegate)
    : CommonPaletteTool(delegate), weak_factory_(this) {
  Shell::Get()->AddPreTargetHandler(this);
  Shell::Get()->AddShellObserver(this);
}

MetalayerMode::~MetalayerMode() {
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->RemovePreTargetHandler(this);
}

PaletteGroup MetalayerMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId MetalayerMode::GetToolId() const {
  return PaletteToolId::METALAYER;
}

void MetalayerMode::OnEnable() {
  CommonPaletteTool::OnEnable();

  Shell::Get()->palette_delegate()->ShowMetalayer(
      base::Bind(&MetalayerMode::OnMetalayerDone, weak_factory_.GetWeakPtr()));
  delegate()->HidePalette();
}

void MetalayerMode::OnDisable() {
  CommonPaletteTool::OnDisable();

  Shell::Get()->palette_delegate()->HideMetalayer();
}

const gfx::VectorIcon& MetalayerMode::GetActiveTrayIcon() const {
  return kPaletteTrayIconMetalayerIcon;
}

const gfx::VectorIcon& MetalayerMode::GetPaletteIcon() const {
  return kPaletteModeMetalayerIcon;
}

views::View* MetalayerMode::CreateView() {
  views::View* view = CreateDefaultView(base::string16());
  UpdateView();
  return view;
}

void MetalayerMode::OnMetalayerDone() {
  delegate()->DisableTool(GetToolId());
}

void MetalayerMode::OnTouchEvent(ui::TouchEvent* event) {
  if (event->pointer_details().pointer_type !=
      ui::EventPointerType::POINTER_TYPE_PEN)
    return;

  if (event->type() != ui::ET_TOUCH_PRESSED)
    return;

  // The stylus "barrel" button press is encoded as ui::EF_LEFT_MOUSE_BUTTON
  if (!(event->flags() & ui::EF_LEFT_MOUSE_BUTTON))
    return;

  if (palette_utils::PaletteContainsPointInScreen(event->root_location()))
    return;

  if (enabled())
    return;

  if (voice_interaction_state_ == ash::VoiceInteractionState::NOT_READY) {
    // Repetitive presses will create toasts with the same id which will be
    // ignored.
    ToastData toast(
        kToastId,
        l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_METALAYER_TOAST_LOADING),
        kToastDurationMs, base::Optional<base::string16>());
    Shell::Get()->toast_manager()->Show(toast);
    event->StopPropagation();
    return;
  }
  // TODO(kaznacheev) When DISABLED state is introduced, check it here and
  // bail out.

  // TODO(kaznacheev) Use prefs instead of IsMetalayerSupported
  // when crbug.com/727873 is fixed.
  // Shell::palette_delegate() might return null in some tests that are not
  // concerned with palette but generate touch events.
  if (!Shell::Get()->palette_delegate() ||
      !Shell::Get()->palette_delegate()->IsMetalayerSupported())
    return;

  delegate()->RecordPaletteOptionsUsage(
      PaletteToolIdToPaletteTrayOptions(GetToolId()),
      PaletteInvocationMethod::SHORTCUT);
  delegate()->EnableTool(GetToolId());
  event->StopPropagation();
}

void MetalayerMode::OnVoiceInteractionStatusChanged(
    ash::VoiceInteractionState state) {
  voice_interaction_state_ = state;
  UpdateView();
  if (voice_interaction_state_ != ash::VoiceInteractionState::NOT_READY)
    Shell::Get()->toast_manager()->Cancel(kToastId);
}

void MetalayerMode::UpdateView() {
  if (!highlight_view_)
    return;

  const bool ready =
      voice_interaction_state_ != ash::VoiceInteractionState::NOT_READY;

  // TODO(kaznacheev) Use prefs instead of IsMetalayerSupported
  // when crbug.com/727873 is fixed.
  const bool supported =
      Shell::Get()->palette_delegate() &&
      Shell::Get()->palette_delegate()->IsMetalayerSupported();

  highlight_view_->text_label()->SetText(l10n_util::GetStringUTF16(
      ready ? IDS_ASH_STYLUS_TOOLS_METALAYER_MODE
            : IDS_ASH_STYLUS_TOOLS_METALAYER_MODE_LOADING));

  highlight_view_->SetEnabled(ready && supported);

  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  style.set_color_style(highlight_view_->enabled()
                            ? TrayPopupItemStyle::ColorStyle::ACTIVE
                            : TrayPopupItemStyle::ColorStyle::DISABLED);

  style.SetupLabel(highlight_view_->text_label());

  highlight_view_->left_icon()->SetImage(
      CreateVectorIcon(GetPaletteIcon(), kMenuIconSize, style.GetIconColor()));
}

}  // namespace ash
