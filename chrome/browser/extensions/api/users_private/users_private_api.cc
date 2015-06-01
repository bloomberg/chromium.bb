// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/users_private/users_private_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateGetWhitelistedUsersFunction

UsersPrivateGetWhitelistedUsersFunction::
    UsersPrivateGetWhitelistedUsersFunction() {
}

UsersPrivateGetWhitelistedUsersFunction::
    ~UsersPrivateGetWhitelistedUsersFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateGetWhitelistedUsersFunction::Run() {
  return RespondNow(OneArgument(new base::ListValue()));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateAddWhitelistedUserFunction

UsersPrivateAddWhitelistedUserFunction::
    UsersPrivateAddWhitelistedUserFunction() {
}

UsersPrivateAddWhitelistedUserFunction::
    ~UsersPrivateAddWhitelistedUserFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateAddWhitelistedUserFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateRemoveWhitelistedUserFunction

UsersPrivateRemoveWhitelistedUserFunction::
    UsersPrivateRemoveWhitelistedUserFunction() {
}

UsersPrivateRemoveWhitelistedUserFunction::
    ~UsersPrivateRemoveWhitelistedUserFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateRemoveWhitelistedUserFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateIsCurrentUserOwnerFunction

UsersPrivateIsCurrentUserOwnerFunction::
    UsersPrivateIsCurrentUserOwnerFunction() {
}

UsersPrivateIsCurrentUserOwnerFunction::
    ~UsersPrivateIsCurrentUserOwnerFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateIsCurrentUserOwnerFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateIsWhitelistManagedFunction

UsersPrivateIsWhitelistManagedFunction::
    UsersPrivateIsWhitelistManagedFunction() {
}

UsersPrivateIsWhitelistManagedFunction::
    ~UsersPrivateIsWhitelistManagedFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateIsWhitelistManagedFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

}  // namespace extensions
