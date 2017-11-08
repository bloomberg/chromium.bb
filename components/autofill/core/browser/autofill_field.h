// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures_util.h"

namespace autofill {

class AutofillType;

class AutofillField : public FormFieldData {
 public:
  enum PhonePart {
    IGNORED = 0,
    PHONE_PREFIX = 1,
    PHONE_SUFFIX = 2,
  };

  AutofillField();
  AutofillField(const FormFieldData& field, const base::string16& unique_name);
  virtual ~AutofillField();

  const base::string16& unique_name() const { return unique_name_; }

  const std::string& section() const { return section_; }
  ServerFieldType heuristic_type() const { return heuristic_type_; }
  ServerFieldType overall_server_type() const { return overall_server_type_; }
  const std::vector<AutofillQueryResponseContents::Field::FieldPrediction>&
  server_predictions() const {
    return server_predictions_;
  }
  HtmlFieldType html_type() const { return html_type_; }
  HtmlFieldMode html_mode() const { return html_mode_; }
  const ServerFieldTypeSet& possible_types() const { return possible_types_; }
  PhonePart phone_part() const { return phone_part_; }
  bool previously_autofilled() const { return previously_autofilled_; }
  const base::string16& parseable_name() const { return parseable_name_; }
  bool only_fill_when_focused() const { return only_fill_when_focused_; }

  // Setters for the detected type and section for this field.
  void set_section(const std::string& section) { section_ = section; }
  void set_heuristic_type(ServerFieldType type);
  void set_overall_server_type(ServerFieldType type);
  void set_server_predictions(
      const std::vector<AutofillQueryResponseContents::Field::FieldPrediction>
          predictions) {
    server_predictions_ = std::move(predictions);
  }
  void set_possible_types(const ServerFieldTypeSet& possible_types) {
    possible_types_ = possible_types;
  }
  void SetHtmlType(HtmlFieldType type, HtmlFieldMode mode);
  void set_previously_autofilled(bool previously_autofilled) {
    previously_autofilled_ = previously_autofilled;
  }
  void set_parseable_name(const base::string16& parseable_name) {
    parseable_name_ = parseable_name;
  }

  void set_only_fill_when_focused(bool fill_when_focused) {
    only_fill_when_focused_ = fill_when_focused;
  }

  // Set the heuristic or server type, depending on whichever is currently
  // assigned, to |type|.
  void SetTypeTo(ServerFieldType type);

  // This function automatically chooses between server and heuristic autofill
  // type, depending on the data available.
  AutofillType Type() const;

  // Returns true if the value of this field is empty.
  bool IsEmpty() const;

  // The unique signature of this field, composed of the field name and the html
  // input type in a 32-bit hash.
  FieldSignature GetFieldSignature() const;

  // Returns the field signature as string.
  std::string FieldSignatureAsStr() const;

  // Returns true if the field type has been determined (without the text in the
  // field).
  bool IsFieldFillable() const;

  void set_default_value(const std::string& value) { default_value_ = value; }
  const std::string& default_value() const { return default_value_; }

  void set_credit_card_number_offset(size_t position) {
    credit_card_number_offset_ = position;
  }
  size_t credit_card_number_offset() const {
    return credit_card_number_offset_;
  }

  void set_generation_type(
      AutofillUploadContents::Field::PasswordGenerationType type) {
    generation_type_ = type;
  }
  AutofillUploadContents::Field::PasswordGenerationType generation_type()
      const {
    return generation_type_;
  }

  void set_generated_password_changed(bool generated_password_changed) {
    generated_password_changed_ = generated_password_changed;
  }
  bool generated_password_changed() const {
    return generated_password_changed_;
  }

  void set_form_classifier_outcome(
      AutofillUploadContents::Field::FormClassifierOutcome outcome) {
    form_classifier_outcome_ = outcome;
  }
  AutofillUploadContents::Field::FormClassifierOutcome form_classifier_outcome()
      const {
    return form_classifier_outcome_;
  }

  void set_username_vote_type(
      AutofillUploadContents::Field::UsernameVoteType type) {
    username_vote_type_ = type;
  }
  AutofillUploadContents::Field::UsernameVoteType username_vote_type() const {
    return username_vote_type_;
  }

 private:
  // Whether the heuristics or server predict a credit card field.
  bool IsCreditCardPrediction() const;

  // The unique name of this field, generated by Autofill.
  base::string16 unique_name_;

  // The unique identifier for the section (e.g. billing vs. shipping address)
  // that this field belongs to.
  std::string section_;

  // The type of the field, as determined by the Autofill server.
  ServerFieldType overall_server_type_;

  // The possible types of the field, as determined by the Autofill server,
  // including |overall_server_type_| as the first item.
  std::vector<AutofillQueryResponseContents::Field::FieldPrediction>
      server_predictions_;

  // The type of the field, as determined by the local heuristics.
  ServerFieldType heuristic_type_;

  // The type of the field, as specified by the site author in HTML.
  HtmlFieldType html_type_;

  // The "mode" of the field, as specified by the site author in HTML.
  // Currently this is used to distinguish between billing and shipping fields.
  HtmlFieldMode html_mode_;

  // The set of possible types for this field.
  ServerFieldTypeSet possible_types_;

  // Used to track whether this field is a phone prefix or suffix.
  PhonePart phone_part_;

  // The default value returned by the Autofill server.
  std::string default_value_;

  // Used to hold the position of the first digit to be copied as a substring
  // from credit card number.
  size_t credit_card_number_offset_;

  // Whether the field was autofilled then later edited.
  bool previously_autofilled_;

  // Whether the field should be filled when it is not the highlighted field.
  bool only_fill_when_focused_;

  // The parseable name attribute, with unnecessary information removed (such as
  // a common prefix shared with other fields). Will be used for heuristics
  // parsing.
  base::string16 parseable_name_;

  // The type of password generation event, if it happened.
  AutofillUploadContents::Field::PasswordGenerationType generation_type_;

  // Whether the generated password was changed by user.
  bool generated_password_changed_;

  // The outcome of HTML parsing based form classifier.
  AutofillUploadContents::Field::FormClassifierOutcome form_classifier_outcome_;

  // The username vote type, if the autofill type is USERNAME. Otherwise, the
  // field is ignored.
  AutofillUploadContents::Field::UsernameVoteType username_vote_type_;

  DISALLOW_COPY_AND_ASSIGN(AutofillField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
