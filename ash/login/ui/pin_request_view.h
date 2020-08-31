// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PIN_REQUEST_VIEW_H_
#define ASH_LOGIN_UI_PIN_REQUEST_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/login/ui/access_code_input.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class LabelButton;
class Textfield;
}  // namespace views

namespace ash {
class ArrowButtonView;
class LoginButton;
class LoginPinView;

// State of the PinRequestView.
enum class PinRequestViewState {
  kNormal,
  kError,
};

struct ASH_EXPORT PinRequest {
  PinRequest();
  PinRequest(PinRequest&&);
  PinRequest& operator=(PinRequest&&);
  ~PinRequest();

  // Callback for PIN validations. It is called when the validation has finished
  // and the view is closing.
  // |success| indicates whether the validation was successful.
  using OnPinRequestDone = base::OnceCallback<void(bool success)>;
  OnPinRequestDone on_pin_request_done = base::NullCallback();

  // Whether the help button is displayed.
  bool help_button_enabled = false;

  base::Optional<int> pin_length;

  // When |pin_keyboard_always_enabled| is set, the PIN keyboard is displayed at
  // all times. Otherwise, it is only displayed when the device is in tablet
  // mode.
  bool pin_keyboard_always_enabled = false;

  // The pin widget is a modal and already contains a dimmer, however
  // when another modal is the parent of the widget, the dimmer will be placed
  // behind the two windows. |extra_dimmer| will create an extra dimmer between
  // the two.
  bool extra_dimmer = false;

  // Whether the entered PIN should be displayed clearly or only as bullets.
  bool obscure_pin = true;

  // Strings for UI.
  base::string16 title;
  base::string16 description;
  base::string16 accessible_title;
};

// The view that allows for input of pins to authorize certain actions.
class ASH_EXPORT PinRequestView : public views::DialogDelegateView,
                                  public views::ButtonListener,
                                  public TabletModeObserver {
 public:
  enum class SubmissionResult {
    // Closes the UI and calls |on_pin_request_done_|.
    kPinAccepted,
    // PIN rejected - keeps the UI in its current state.
    kPinError,
    // Async waiting for result - keeps the UI in its current state.
    kSubmitPending,
  };

  class Delegate {
   public:
    virtual SubmissionResult OnPinSubmitted(const std::string& pin) = 0;
    virtual void OnBack() = 0;
    virtual void OnHelp(gfx::NativeWindow parent_window) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(PinRequestView* view);
    ~TestApi();

    LoginButton* back_button();
    views::Label* title_label();
    views::Label* description_label();
    views::View* access_code_view();
    views::LabelButton* help_button();
    ArrowButtonView* submit_button();
    LoginPinView* pin_keyboard_view();

    views::Textfield* GetInputTextField(int index);

    PinRequestViewState state() const;

   private:
    PinRequestView* const view_;
  };

  // Returns color used for dialog and UI elements specific for child user.
  // |using_blur| should be true if the UI element is using background blur
  // (color transparency depends on it).
  static SkColor GetChildUserDialogColor(bool using_blur);

  // Creates pin request view that will enable the user to enter a pin.
  // |request| is used to configure callbacks and UI details.
  PinRequestView(PinRequest request, Delegate* delegate);
  ~PinRequestView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void RequestFocus() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::DialogDelegateView:
  ui::ModalType GetModalType() const override;
  views::View* GetInitiallyFocusedView() override;
  base::string16 GetAccessibleWindowTitle() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // Sets whether the user can enter a PIN. Other buttons (back, submit etc.)
  // are unaffected.
  void SetInputEnabled(bool input_enabled);

  // Clears previously entered PIN from the PIN input field(s).
  void ClearInput();

  // Updates state of the view.
  void UpdateState(PinRequestViewState state,
                   const base::string16& title,
                   const base::string16& description);

 private:
  class FocusableLabelButton;

  // Submits access code for validation.
  void SubmitCode();

  // Closes the view.
  void OnBack();

  // Updates view's preferred size.
  void UpdatePreferredSize();

  // Moves focus to |submit_button_|.
  void FocusSubmitButton();

  // Called when access code input changes. |complete| brings information
  // whether current input code is complete. |last_field_active| contains
  // information whether last input field is currently active.
  void OnInputChange(bool last_field_active, bool complete);

  // Returns if the pin keyboard should be visible.
  bool PinKeyboardVisible() const;

  // Size that depends on the pin keyboards visibility.
  gfx::Size GetPinRequestViewSize() const;

  PinRequestViewState state_ = PinRequestViewState::kNormal;

  // Unowned pointer to the delegate. The delegate should outlive this instance.
  Delegate* delegate_;

  // Callback to close the UI.
  PinRequest::OnPinRequestDone on_pin_request_done_;

  // Auto submit code when the last input has been inserted.
  bool auto_submit_enabled_ = true;

  // If false, |pin_keyboard_view| is only displayed in tablet mode.
  bool pin_keyboard_always_enabled_ = true;

  // Strings as on view construction to enable restoring the original state.
  base::string16 default_title_;
  base::string16 default_description_;
  base::string16 default_accessible_title_;

  views::Label* title_label_ = nullptr;
  views::Label* description_label_ = nullptr;
  AccessCodeInput* access_code_view_ = nullptr;
  LoginPinView* pin_keyboard_view_ = nullptr;
  LoginButton* back_button_ = nullptr;
  FocusableLabelButton* help_button_ = nullptr;
  ArrowButtonView* submit_button_ = nullptr;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  base::WeakPtrFactory<PinRequestView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PinRequestView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PIN_REQUEST_VIEW_H_
