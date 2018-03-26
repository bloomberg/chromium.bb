// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/caption_buttons/frame_back_button.h"

#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_sink.h"
#include "ui/views/widget/widget.h"

namespace ash {

FrameBackButton::FrameBackButton()
    : FrameCaptionButton(this, CAPTION_BUTTON_ICON_BACK) {
  SetPreferredSize(GetAshLayoutSize(AshLayoutSize::kNonBrowserCaption));
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_WINDOW_CONTROL_ACCNAME_BACK));
}

FrameBackButton::~FrameBackButton() = default;

void FrameBackButton::ButtonPressed(Button* sender, const ui::Event& event) {
  // Send up event as well as down event as ARC++ clients expect this sequence.
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::KeyEvent press_key_event(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_BACK,
                               ui::EF_NONE);
  ignore_result(root_window->GetHost()->event_sink()->OnEventFromSource(
      &press_key_event));
  ui::KeyEvent release_key_event(ui::ET_KEY_RELEASED, ui::VKEY_BROWSER_BACK,
                                 ui::EF_NONE);
  ignore_result(root_window->GetHost()->event_sink()->OnEventFromSource(
      &release_key_event));
}

}  // namespace ash
