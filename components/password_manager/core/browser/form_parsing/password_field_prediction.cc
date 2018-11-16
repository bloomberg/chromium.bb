// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

#include "base/logging.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures_util.h"

using autofill::FieldSignature;
using autofill::FormData;
using autofill::FormStructure;
using autofill::ServerFieldType;

namespace password_manager {

namespace {

// Returns true if the field is password or username prediction.
bool IsCredentialRelatedPrediction(ServerFieldType type) {
  return DeriveFromServerFieldType(type) != CredentialFieldType::kNone;
}

}  // namespace

CredentialFieldType DeriveFromServerFieldType(ServerFieldType type) {
  switch (type) {
    case autofill::USERNAME:
    case autofill::USERNAME_AND_EMAIL_ADDRESS:
      return CredentialFieldType::kUsername;
    case autofill::PASSWORD:
      return CredentialFieldType::kCurrentPassword;
    case autofill::ACCOUNT_CREATION_PASSWORD:
    case autofill::NEW_PASSWORD:
      return CredentialFieldType::kNewPassword;
    case autofill::CONFIRMATION_PASSWORD:
      return CredentialFieldType::kConfirmationPassword;
    default:
      return CredentialFieldType::kNone;
  }
}

FormPredictions ConvertToFormPredictions(const FormStructure& form_structure) {
  // This is a mostly mechanical transformation, except for the following case:
  // If there is no explicit CONFIRMATION_PASSWORD field, and there are two
  // fields with the same signature and one of the "new password" types, then
  // the latter of those two should be marked as CONFIRMATION_PASSWORD. For
  // fields which have the same signature, the server has no means to hint
  // different types, and it is likely that one of them is the confirmation
  // field.

  // Stores the signature of the last field with the server type
  // ACCOUNT_CREATION_PASSWORD or NEW_PASSWORD. The value 0 represents "no
  // field with the 'new password' type seen yet".
  FieldSignature last_new_password = 0;

  bool explicit_confirmation_hint_present = false;
  for (const auto& field : form_structure) {
    if (field->server_type() == autofill::CONFIRMATION_PASSWORD) {
      explicit_confirmation_hint_present = true;
      break;
    }
  }

  FormPredictions result;
  for (const auto& field : form_structure) {
    ServerFieldType server_type = field->server_type();

    if (!explicit_confirmation_hint_present &&
        (server_type == autofill::ACCOUNT_CREATION_PASSWORD ||
         server_type == autofill::NEW_PASSWORD)) {
      FieldSignature current_signature = field->GetFieldSignature();
      if (last_new_password && last_new_password == current_signature) {
        server_type = autofill::CONFIRMATION_PASSWORD;
      } else {
        last_new_password = current_signature;
      }
    }

    if (IsCredentialRelatedPrediction(server_type)) {
      bool may_use_prefilled_placeholder = false;
      for (const auto& predictions : field->server_predictions()) {
        may_use_prefilled_placeholder |=
            predictions.may_use_prefilled_placeholder();
      }

      result[field->unique_renderer_id] = PasswordFieldPrediction{
          .type = server_type,
          .may_use_prefilled_placeholder = may_use_prefilled_placeholder};
    }
  }

  return result;
}

}  // namespace password_manager
