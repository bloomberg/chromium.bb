// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_REQUEST_PIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_UI_REQUEST_PIN_VIEW_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/certificate_provider/security_token_pin_dialog_host.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}

namespace chromeos {

// A dialog box for requesting PIN code. Instances of this class are managed by
// SecurityTokenPinDialogHostPopupImpl.
class RequestPinView final : public views::DialogDelegateView,
                             public views::TextfieldController {
 public:
  using PinEnteredCallback =
      base::RepeatingCallback<void(const std::string& user_input)>;
  using ViewDestructionCallback = base::OnceClosure;

  // Creates the view to be embedded in the dialog that requests the PIN/PUK.
  // |extension_name| - the name of the extension making the request. Displayed
  //     in the title and in the header of the view.
  // |code_type| - the type of code requested, PIN or PUK.
  // |attempts_left| - the number of attempts user has to try the code. When
  //     zero the textfield is disabled and user cannot provide any input. When
  //     -1 the user is allowed to provide the input and no information about
  //     the attepts left is displayed in the view.
  // |pin_entered_callback| - called every time the user submits the input.
  // |view_destruction_callback| - called by the destructor.
  RequestPinView(const std::string& extension_name,
                 security_token_pin::CodeType code_type,
                 int attempts_left,
                 const PinEnteredCallback& pin_entered_callback,
                 ViewDestructionCallback view_destruction_callback);
  RequestPinView(const RequestPinView&) = delete;
  RequestPinView& operator=(const RequestPinView&) = delete;
  ~RequestPinView() override;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // views::DialogDelegate
  bool Accept() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // views::View
  gfx::Size CalculatePreferredSize() const override;

  // |code_type| - specifies whether the user is asked to enter PIN or PUK.
  // |error_label| - the error template to be displayed in red in the dialog. If
  //     |kNone|, no error is displayed.
  // |attempts_left| - included in the view as the number of attepts user can
  //     have to enter correct code.
  // |accept_input| - specifies whether the textfield is enabled. If disabled
  //     the user is unable to provide input.
  void SetDialogParameters(security_token_pin::CodeType code_type,
                           security_token_pin::ErrorLabel error_label,
                           int attempts_left,
                           bool accept_input);

  // Set the name of extension that is using this view. The name is included in
  // the header text displayed by the view.
  void SetExtensionName(const std::string& extension_name);

  views::Textfield* textfield_for_testing() { return textfield_; }
  views::Label* error_label_for_testing() { return error_label_; }

 private:
  // This initializes the view, with all the UI components.
  void Init();
  void SetAcceptInput(bool accept_input);
  void SetErrorMessage(security_token_pin::ErrorLabel error_label,
                       int attempts_left,
                       bool accept_input);
  // Updates the header text |header_label_| based on values from
  // |window_title_| and |code_type_|.
  void UpdateHeaderText();

  const PinEnteredCallback pin_entered_callback_;
  ViewDestructionCallback view_destruction_callback_;

  // Whether the UI is locked, disallowing the user to input any data, while the
  // caller processes the previously entered PIN/PUK.
  bool locked_ = false;

  base::string16 window_title_;
  views::Label* header_label_ = nullptr;
  base::string16 code_type_;
  views::Textfield* textfield_ = nullptr;
  views::Label* error_label_ = nullptr;

  base::WeakPtrFactory<RequestPinView> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_REQUEST_PIN_VIEW_H_
