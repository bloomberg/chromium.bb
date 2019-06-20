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

  // Creates parent access view that will validate the parent access code for a
  // specific child, when |account_id| is set, or to any child signed in the
  // device, when it is empty. |callbacks| will be called when user performs
  // certain actions.
  ParentAccessView(const base::Optional<AccountId>& account_id,
                   const Callbacks& callbacks);
  ~ParentAccessView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void RequestFocus() override;
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

  // Updates view's preferred size.
  void UpdatePreferredSize();

  // Called when access code input changes. |complete| brings information
  // whether current input code is complete.
  void OnInputChange(bool complete);

  // Callbacks to be called when user performs certain actions.
  const Callbacks callbacks_;

  // Account id of the user that parent access code is processed for. When
  // empty, the code is processed for all the children signed in the device.
  const base::Optional<AccountId> account_id_;

  State state_ = State::kNormal;

  views::Label* title_label_ = nullptr;
  views::Label* description_label_ = nullptr;
  AccessCodeInput* access_code_view_ = nullptr;
  LoginPinView* pin_keyboard_view_ = nullptr;
  LoginButton* back_button_ = nullptr;
  views::LabelButton* help_button_ = nullptr;
  ArrowButtonView* submit_button_ = nullptr;
  NonAccessibleView* pin_keyboard_to_footer_spacer_ = nullptr;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ParentAccessView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_
