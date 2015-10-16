// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/password_manager/core/common/experiments.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateCanPasswordAccountBeManagedFunction

PasswordsPrivateCanPasswordAccountBeManagedFunction::
    ~PasswordsPrivateCanPasswordAccountBeManagedFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateCanPasswordAccountBeManagedFunction::Run() {
  scoped_ptr<base::FundamentalValue> visible(new base::FundamentalValue(true));
  return RespondNow(OneArgument(visible.Pass()));
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

  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);
  delegate->RemoveSavedPassword(
      parameters->login_pair.origin_url,
      parameters->login_pair.username);

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

  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);
  delegate->RemovePasswordException(parameters->exception_url);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateRequestPlaintextPasswordFunction

PasswordsPrivateRequestPlaintextPasswordFunction::
    ~PasswordsPrivateRequestPlaintextPasswordFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateRequestPlaintextPasswordFunction::Run() {
  scoped_ptr<api::passwords_private::RequestPlaintextPassword::Params>
      parameters = api::passwords_private::RequestPlaintextPassword::Params::
          Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);

  delegate->RequestShowPassword(parameters->login_pair.origin_url,
                                parameters->login_pair.username,
                                GetSenderWebContents());

  // No response given from this API function; instead, listeners wait for the
  // chrome.passwordsPrivate.onPlaintextPasswordRetrieved event to fire.
  return RespondNow(NoArguments());
}

}  // namespace extensions
