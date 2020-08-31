// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_ECHO_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_UI_ECHO_DIALOG_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class ImageButton;
class View;
}  // namespace views

namespace gfx {
class FontList;
}  // namespace gfx

namespace chromeos {

class EchoDialogListener;

// Dialog shown by echoPrivate extension API when getUserConsent function is
// called. The API is used by echo extension when an offer from a service is
// being redeemed. The dialog is shown to get an user consent. If the echo
// extension is not allowed by policy to redeem offers, the dialog informs user
// about this.
class EchoDialogView : public views::DialogDelegateView,
                       public views::ButtonListener {
 public:
  struct Params {
    bool echo_enabled = false;
    base::string16 service_name;
    base::string16 origin;
  };

  EchoDialogView(EchoDialogListener* listener, const Params& params);
  ~EchoDialogView() override;

  // Shows the dialog.
  void Show(gfx::NativeWindow parent);

 private:
  friend class ExtensionEchoPrivateApiTest;

  // Initializes dialog layout that will be showed when echo extension is
  // allowed to redeem offers. |service_name| is the name of the service that
  // requests user consent to redeem an offer. |origin| is the service's origin
  // url. Service name should be underlined in the dialog, and hovering over its
  // label should display tooltip containing |origin|.
  // The dialog will have both OK and Cancel buttons.
  void InitForEnabledEcho(const base::string16& service_name, const base::string16& origin);

  // Initializes dialog layout that will be shown when echo extension is not
  // allowed to redeem offers. The dialog will be showing a message that the
  // offer redeeming is disabled by policy.
  // The dialog will have only Cancel button.
  void InitForDisabledEcho();

  // views::DialogDelegateView:
  ui::ModalType GetModalType() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Sets the border and label view.
  void SetBorderAndLabel(std::unique_ptr<views::View> label,
                         const gfx::FontList& label_font_list);

  EchoDialogListener* listener_ = nullptr;
  views::ImageButton* learn_more_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EchoDialogView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_ECHO_DIALOG_VIEW_H_
