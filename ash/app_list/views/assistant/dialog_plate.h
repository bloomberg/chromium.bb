// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_DIALOG_PLATE_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_DIALOG_PLATE_H_

#include <memory>
#include <string>

#include "ash/app_list/app_list_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query_history.h"
#include "ash/assistant/ui/dialog_plate/action_view.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ash {
class ActionView;
enum class AssistantButtonId;
class AssistantViewDelegate;
class BaseLogoView;
class DialogPlateObserver;
}  // namespace ash

namespace ui {
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace views {
class ImageButton;
}  // namespace views

namespace app_list {

// DialogPlate -----------------------------------------------------------------

// DialogPlate is the child of AssistantMainView concerned with providing the
// means by which a user converses with Assistant. To this end, DialogPlate
// provides a textfield for use with the keyboard input modality, an
// ActionView which serves to either commit a text query, or toggle voice
// interaction as appropriate for the user's current input modality, and a back
// button to go back in the launcher.
class APP_LIST_EXPORT DialogPlate
    : public views::View,
      public views::TextfieldController,
      public ash::AssistantInteractionModelObserver,
      public views::ButtonListener {
 public:
  explicit DialogPlate(ash::AssistantViewDelegate* delegate);
  ~DialogPlate() override;

  // Adds/removes the specified |observer|.
  void AddObserver(ash::DialogPlateObserver* observer);
  void RemoveObserver(ash::DialogPlateObserver* observer);

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void RequestFocus() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // ash::AssistantInteractionModelObserver:
  void OnInputModalityChanged(ash::InputModality input_modality) override;
  void OnCommittedQueryChanged(
      const ash::AssistantQuery& committed_query) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

 private:
  void InitLayout();
  void InitKeyboardLayoutContainer();
  void InitVoiceLayoutContainer();

  void OnButtonPressed(ash::AssistantButtonId id);

  void OnAnimationStarted(const ui::CallbackLayerAnimationObserver& observer);
  bool OnAnimationEnded(const ui::CallbackLayerAnimationObserver& observer);

  void SetFocus(ash::InputModality modality);

  ash::AssistantViewDelegate* const delegate_;

  views::ImageButton* back_button_;               // Owned by view hierarchy.
  ash::BaseLogoView* molecule_icon_;              // Owned by view hierarchy.
  views::View* input_modality_layout_container_;  // Owned by view hierarchy.
  views::View* keyboard_layout_container_;        // Owned by view hierarchy.
  views::View* voice_layout_container_;           // Owned by view hierarchy.
  views::ImageButton* keyboard_input_toggle_;     // Owned by view hierarchy.
  views::ImageButton* voice_input_toggle_;        // Owned by view hierarchy.
  ash::ActionView* animated_voice_input_toggle_;  // Owned by view hierarchy.
  views::Textfield* textfield_;                   // Owned by view hierarchy.

  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;
  std::unique_ptr<ash::AssistantQueryHistory::Iterator> query_history_iterator_;

  base::ObserverList<ash::DialogPlateObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DialogPlate);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_DIALOG_PLATE_H_
