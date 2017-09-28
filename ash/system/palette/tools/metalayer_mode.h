// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_
#define ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/voice_interaction_state.h"
#include "ash/shell_observer.h"
#include "ash/system/palette/common_palette_tool.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event_handler.h"

namespace ash {

// A palette tool that lets the user select a screen region to be passed
// to the voice interaction framework.
//
// Unlike other palette tools, it can be activated not only through the stylus
// menu, but also by the stylus button click.
class ASH_EXPORT MetalayerMode : public CommonPaletteTool,
                                 public ui::EventHandler,
                                 public ShellObserver {
 public:
  explicit MetalayerMode(Delegate* delegate);
  ~MetalayerMode() override;

 private:
  // Whether the metalayer feature is enabled by the user. This is different
  // from |enabled| which means that the palette tool is currently selected by
  // the user.
  bool feature_enabled() const {
    return voice_interaction_enabled_ && voice_interaction_context_enabled_;
  }

  // Whether the tool is in "loading" state.
  bool loading() const {
    return feature_enabled() &&
           voice_interaction_state_ == VoiceInteractionState::NOT_READY;
  }

  // Whether the tool can be selected from the menu (only true when enabled
  // by the user and fully loaded).
  bool selectable() const {
    return feature_enabled() &&
           voice_interaction_state_ != VoiceInteractionState::NOT_READY;
  }

  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  void OnDisable() override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;
  views::View* CreateView() override;

  // CommonPaletteTool:
  const gfx::VectorIcon& GetPaletteIcon() const override;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // ShellObserver:
  void OnVoiceInteractionStatusChanged(
      ash::VoiceInteractionState state) override;
  void OnVoiceInteractionEnabled(bool enabled) override;
  void OnVoiceInteractionContextEnabled(bool enabled) override;

  // Update the state of the tool based on the current availability of the tool.
  void UpdateState();

  // Update the palette menu item based on the current availability of the tool.
  void UpdateView();

  // Called when the metalayer session is complete.
  void OnMetalayerSessionComplete();

  ash::VoiceInteractionState voice_interaction_state_ =
      ash::VoiceInteractionState::NOT_READY;

  bool voice_interaction_enabled_ = false;

  bool voice_interaction_context_enabled_ = false;

  base::TimeTicks previous_stroke_end_;

  // True when the mode is activated via the stylus barrel button.
  bool activated_via_button_ = false;

  base::WeakPtrFactory<MetalayerMode> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetalayerMode);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_
