// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_
#define ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "components/account_id/account_id.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {
class ArrowButtonView;
class LoginButton;
class LoginPinView;

// The view that allows for input of parent access code to authorize certain
// actions on child's device.
class ASH_EXPORT ParentAccessView : public NonAccessibleView,
                                    public views::ButtonListener,
                                    public TabletModeObserver {
 public:
  // ParentAccessView state.
  enum class State {
    kNormal,  // View with default texts and colors.
    kError    // View with texts and color signalizing input error.
  };

  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(ParentAccessView* view);
    ~TestApi();

    LoginButton* back_button() const;
    views::Label* title_label() const;
    views::Label* description_label() const;
    views::View* access_code_view() const;
    views::LabelButton* help_button() const;
    ArrowButtonView* submit_button() const;
    LoginPinView* pin_keyboard_view() const;

    State state() const;

   private:
    ParentAccessView* const view_;
  };

  using OnFinished = base::RepeatingCallback<void(bool access_granted)>;

  // Parent access view callbacks.
  struct Callbacks {
    Callbacks();
    Callbacks(const Callbacks& other);
    ~Callbacks();

    // Called when ParentAccessView finshed processing and should be dismissed.
    // If access code was successfully validated, |access_granted| will
    // contain true. If access code was not entered or not successfully
    // validated and user pressed back button, |access_granted| will contain
    // false.
    OnFinished on_finished;
  };

  // Creates parent access view for the user identified by |account_id|.
  // |callbacks| will be called when user performs certain actions.
  ParentAccessView(const AccountId& account_id, const Callbacks& callbacks);
  ~ParentAccessView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void RequestFocus() override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

 private:
  class AccessCodeInput;

  // Submits access code for validation.
  void SubmitCode();

  // Updates state of the view.
  void UpdateState(State state);

  // Called when access code input changes. |complete| brings information
  // whether current input code is complete.
  void OnInputChange(bool complete);

  // To be called when parent access code validation was completed. Result of
  // the validation is available in |result| if validation was performed.
  void OnValidationResult(base::Optional<bool> result);

  // Callbacks to be called when user performs certain actions.
  const Callbacks callbacks_;

  // Account id of the user that parent access code is processed for.
  const AccountId account_id_;

  State state_ = State::kNormal;

  views::Label* title_label_ = nullptr;
  views::Label* description_label_ = nullptr;
  AccessCodeInput* access_code_view_ = nullptr;
  LoginPinView* pin_keyboard_view_ = nullptr;
  LoginButton* back_button_ = nullptr;
  views::LabelButton* help_button_ = nullptr;
  ArrowButtonView* submit_button_ = nullptr;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  base::WeakPtrFactory<ParentAccessView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ParentAccessView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_
