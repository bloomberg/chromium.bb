// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/autofill/core/browser/field_types.h"
#include "ui/views/controls/button/vector_icon_button_delegate.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace views {
class Textfield;
class View;
}  // namespace views

namespace payments {

class PaymentRequest;
class PaymentRequestDialogView;

// Field definition for an editor field, used to build the UI.
struct EditorField {
  enum class LengthHint : int { HINT_LONG, HINT_SHORT };

  EditorField(autofill::ServerFieldType type,
              const base::string16& label,
              LengthHint length_hint)
      : type(type), label(label), length_hint(length_hint) {}

  const autofill::ServerFieldType type;
  const base::string16 label;
  LengthHint length_hint;
};

// The PaymentRequestSheetController subtype for the editor screens of the
// Payment Request flow.
class EditorViewController : public PaymentRequestSheetController,
                             public views::TextfieldController {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  EditorViewController(PaymentRequest* request,
                       PaymentRequestDialogView* dialog);
  ~EditorViewController() override;

  // PaymentRequestSheetController:
  std::unique_ptr<views::View> CreateView() override;

  // Returns the field definitions used to build the UI.
  virtual std::vector<EditorField> GetFieldDefinitions() = 0;
  // Validates the data entered and attempts to save; returns true on success.
  virtual bool ValidateModelAndSave() = 0;

 private:
  // PaymentRequestSheetController:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // Creates a view for an input field to be added in the editor sheet. |field|
  // is the field definition, which contains the label and the hint about
  // the length of the input field.
  std::unique_ptr<views::View> CreateInputField(const EditorField& field,
                                                views::Textfield** text_field);

  // Used to remember the association between the input field UI element and the
  // original field definition. The Textfield* are owned by their parent view,
  // this only keeps a reference that is good as long as the Textfield is
  // visible.
  std::unordered_map<views::Textfield*, const EditorField> text_fields_;

  DISALLOW_COPY_AND_ASSIGN(EditorViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_
