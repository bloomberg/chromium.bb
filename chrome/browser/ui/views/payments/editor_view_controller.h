// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_

#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "components/autofill/core/browser/field_types.h"
#include "ui/views/controls/button/vector_icon_button_delegate.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ui {
class ComboboxModel;
}

namespace views {
class GridLayout;
class Label;
class Textfield;
class View;
}  // namespace views

namespace payments {

class PaymentRequest;
class PaymentRequestDialogView;
class ValidatingCombobox;
class ValidatingTextfield;

// Field definition for an editor field, used to build the UI.
struct EditorField {
  enum class LengthHint : int { HINT_LONG, HINT_SHORT };
  enum class ControlType : int { TEXTFIELD, COMBOBOX };

  EditorField(autofill::ServerFieldType type,
              const base::string16& label,
              LengthHint length_hint,
              bool required,
              ControlType control_type = ControlType::TEXTFIELD)
      : type(type),
        label(label),
        length_hint(length_hint),
        required(required),
        control_type(control_type) {}

  struct Compare {
    bool operator()(const EditorField& lhs, const EditorField& rhs) const {
      return std::tie(lhs.type, lhs.label) < std::tie(rhs.type, rhs.label);
    }
  };

  // Data type in the field.
  const autofill::ServerFieldType type;
  // Label to be shown alongside the field.
  const base::string16 label;
  // Hint about the length of this field's contents.
  LengthHint length_hint;
  // Whether the field is required.
  bool required;
  // The control type.
  ControlType control_type;
};

// The PaymentRequestSheetController subtype for the editor screens of the
// Payment Request flow.
class EditorViewController : public PaymentRequestSheetController,
                             public views::TextfieldController,
                             public views::ComboboxListener {
 public:
  using TextFieldsMap =
      std::unordered_map<ValidatingTextfield*, const EditorField>;
  using ComboboxMap =
      std::unordered_map<ValidatingCombobox*, const EditorField>;
  using ErrorLabelMap =
      std::map<const EditorField, views::Label*, EditorField::Compare>;

  // Does not take ownership of the arguments, which should outlive this object.
  EditorViewController(PaymentRequest* request,
                       PaymentRequestDialogView* dialog);
  ~EditorViewController() override;

  // PaymentRequestSheetController:
  std::unique_ptr<views::View> CreateView() override;
  std::unique_ptr<views::View> CreateExtraFooterView() override;

  virtual std::unique_ptr<views::View> CreateHeaderView() = 0;
  // Returns the field definitions used to build the UI.
  virtual std::vector<EditorField> GetFieldDefinitions() = 0;
  // Validates the data entered and attempts to save; returns true on success.
  virtual bool ValidateModelAndSave() = 0;
  // Creates a ValidationDelegate which knows how to validate for a given
  // |field| definition.
  virtual std::unique_ptr<ValidationDelegate> CreateValidationDelegate(
      const EditorField& field) = 0;
  virtual std::unique_ptr<ui::ComboboxModel> GetComboboxModelForType(
      const autofill::ServerFieldType& type) = 0;

  // Will display |error_message| alongside the input field represented by
  // |field|.
  void DisplayErrorMessageForField(const EditorField& field,
                                   const base::string16& error_message);

  const ComboboxMap& comboboxes() const { return comboboxes_; }
  const TextFieldsMap& text_fields() const { return text_fields_; }

 protected:
  // PaymentRequestSheetController;
  std::unique_ptr<views::Button> CreatePrimaryButton() override;

 private:
  // PaymentRequestSheetController:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // views::ComboboxListener:
  void OnPerformAction(views::Combobox* combobox) override;

  // Creates the whole editor view to go within the editor dialog. It
  // encompasses all the input fields created by CreateInputField().
  std::unique_ptr<views::View> CreateEditorView();

  // Adds some views to |layout|, to represent an input field and its labels.
  // |field| is the field definition, which contains the label and the hint
  // about the length of the input field. A placeholder error label is also
  // added (see implementation).
  void CreateInputField(views::GridLayout* layout, const EditorField& field);

  // Used to remember the association between the input field UI element and the
  // original field definition. The ValidatingTextfield* and ValidatingCombobox*
  // are owned by their parent view, this only keeps a reference that is good as
  // long as the input field is visible.
  TextFieldsMap text_fields_;
  ComboboxMap comboboxes_;
  // Tracks the relationship between a field and its error label.
  ErrorLabelMap error_labels_;

  DISALLOW_COPY_AND_ASSIGN(EditorViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_
