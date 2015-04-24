// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include "base/values.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveAddressFunction

AutofillPrivateSaveAddressFunction::~AutofillPrivateSaveAddressFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateSaveAddressFunction::Run() {
  scoped_ptr<api::autofill_private::SaveAddress::Params> parameters =
      api::autofill_private::SaveAddress::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivate*Function

AutofillPrivateGetAddressComponentsFunction::
    ~AutofillPrivateGetAddressComponentsFunction() {}

ExtensionFunction::ResponseAction
    AutofillPrivateGetAddressComponentsFunction::Run() {
  scoped_ptr<api::autofill_private::GetAddressComponents::Params> parameters =
      api::autofill_private::GetAddressComponents::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveCreditCardFunction

AutofillPrivateSaveCreditCardFunction::
    ~AutofillPrivateSaveCreditCardFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateSaveCreditCardFunction::Run() {
  scoped_ptr<api::autofill_private::SaveCreditCard::Params> parameters =
      api::autofill_private::SaveCreditCard::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveEntryFunction

AutofillPrivateRemoveEntryFunction::~AutofillPrivateRemoveEntryFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateRemoveEntryFunction::Run() {
  scoped_ptr<api::autofill_private::RemoveEntry::Params> parameters =
      api::autofill_private::RemoveEntry::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateValidatePhoneNumbersFunction

AutofillPrivateValidatePhoneNumbersFunction::
    ~AutofillPrivateValidatePhoneNumbersFunction() {}

ExtensionFunction::ResponseAction
    AutofillPrivateValidatePhoneNumbersFunction::Run() {
  scoped_ptr<api::autofill_private::ValidatePhoneNumbers::Params> parameters =
      api::autofill_private::ValidatePhoneNumbers::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateMaskCreditCardFunction

AutofillPrivateMaskCreditCardFunction::
    ~AutofillPrivateMaskCreditCardFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateMaskCreditCardFunction::Run() {
  scoped_ptr<api::autofill_private::MaskCreditCard::Params> parameters =
      api::autofill_private::MaskCreditCard::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

}  // namespace extensions
