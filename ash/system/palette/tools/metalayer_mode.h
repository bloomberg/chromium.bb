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

  void OnMetalayerDone();

  // Update the palette menu item based on the current availability of the tool.
  void UpdateView();

  ash::VoiceInteractionState voice_interaction_state_ =
      ash::VoiceInteractionState::NOT_READY;

  base::WeakPtrFactory<MetalayerMode> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetalayerMode);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_
