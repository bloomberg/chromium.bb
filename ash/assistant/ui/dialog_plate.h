// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ash {

class AshAssistantController;

class DialogPlate : public views::View,
                    public views::TextfieldController,
                    public AssistantInteractionModelObserver {
 public:
  explicit DialogPlate(AshAssistantController* assistant_controller);
  ~DialogPlate() override;

  // views::View:
  void RequestFocus() override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnMicStateChanged(MicState mic_state) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  void InitLayout();
  void UpdateIcon();

  AshAssistantController* const assistant_controller_;  // Owned by Shell.
  views::Textfield* textfield_;  // Owned by view hierarchy.
  views::View* icon_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(DialogPlate);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_H_
