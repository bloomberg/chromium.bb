// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_

#include <string>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/ui/dialog_plate/action_view.h"
#include "base/macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ash {

class AssistantController;

// DialogPlateDelegate ---------------------------------------------------------

class DialogPlateDelegate {
 public:
  // Invoked on dialog plate action pressed event.
  virtual void OnDialogPlateActionPressed(const std::string& text) {}

  // Invoked on dialog plate contents changed event.
  virtual void OnDialogPlateContentsChanged(const std::string& text) {}

  // Invoked on dialog plate contents committed event.
  virtual void OnDialogPlateContentsCommitted(const std::string& text) {}

 protected:
  virtual ~DialogPlateDelegate() = default;
};

// DialogPlate -----------------------------------------------------------------

// DialogPlate is the child of AssistantMainView concerned with providing the
// means by which a user converses with Assistant. To this end, DialogPlate
// provides a textfield for use with the keyboard input modality, and an
// ActionView which serves to either commit a text query, or toggle voice
// interaction as appropriate for the user's current input modality.
class DialogPlate : public views::View,
                    public views::TextfieldController,
                    public ActionViewListener,
                    public AssistantInteractionModelObserver {
 public:
  explicit DialogPlate(AssistantController* assistant_controller);
  ~DialogPlate() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void RequestFocus() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // ActionViewListener:
  void OnActionPressed() override;

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;

  void set_delegate(DialogPlateDelegate* delegate) { delegate_ = delegate; }

 private:
  void InitLayout();
  void UpdateIcon();

  AssistantController* const assistant_controller_;  // Owned by Shell.
  views::Textfield* textfield_;                      // Owned by view hierarchy.
  ActionView* action_view_;                          // Owned by view hierarchy.

  DialogPlateDelegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DialogPlate);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_
