// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_H_

#include "base/macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ash {

class AshAssistantController;

class DialogPlate : public views::View, public views::TextfieldController {
 public:
  explicit DialogPlate(AshAssistantController* assistant_controller);
  ~DialogPlate() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  void InitLayout();

  AshAssistantController* const assistant_controller_;  // Owned by Shell.

  DISALLOW_COPY_AND_ASSIGN(DialogPlate);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_H_
