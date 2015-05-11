// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_api.h"

#include "base/values.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateCanPasswordAccountBeManagedFunction

PasswordsPrivateCanPasswordAccountBeManagedFunction::
    ~PasswordsPrivateCanPasswordAccountBeManagedFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateCanPasswordAccountBeManagedFunction::Run() {
  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateRemoveSavedPasswordFunction

PasswordsPrivateRemoveSavedPasswordFunction::
    ~PasswordsPrivateRemoveSavedPasswordFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateRemoveSavedPasswordFunction::Run() {
  scoped_ptr<api::passwords_private::RemoveSavedPassword::Params>
      parameters = api::passwords_private::RemoveSavedPassword::Params::
          Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateRemovePasswordExceptionFunction

PasswordsPrivateRemovePasswordExceptionFunction::
    ~PasswordsPrivateRemovePasswordExceptionFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateRemovePasswordExceptionFunction::Run() {
  scoped_ptr<api::passwords_private::RemovePasswordException::Params>
      parameters = api::passwords_private::RemovePasswordException::
          Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateGetPlaintextPasswordFunction

PasswordsPrivateGetPlaintextPasswordFunction::
    ~PasswordsPrivateGetPlaintextPasswordFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateGetPlaintextPasswordFunction::Run() {
  scoped_ptr<api::passwords_private::GetPlaintextPassword::Params>
      parameters = api::passwords_private::GetPlaintextPassword::Params::
          Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // TODO(khorimoto): Implement.

  return RespondNow(NoArguments());
}

}  // namespace extensions
