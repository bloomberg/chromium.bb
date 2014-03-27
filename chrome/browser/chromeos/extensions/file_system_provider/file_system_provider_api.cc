// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider.h"

namespace extensions {
namespace {

// Error names from
// http://www.w3.org/TR/file-system-api/#errors-and-exceptions
const char kSecurityErrorName[] = "SecurityError";

// Error messages.
const char kEmptyNameErrorMessage[] = "Empty display name is not allowed.";
const char kRegisteringFailedErrorMessage[] =
    "Registering the file system failed.";

// Creates a dictionary, which looks like a DOMError. The returned dictionary
// will be converted to a real DOMError object in
// file_system_provier_custom_bindings.js.
base::DictionaryValue* CreateError(const std::string& name,
                                   const std::string& message) {
  base::DictionaryValue* error = new base::DictionaryValue();
  error->SetString("name", name);
  error->SetString("message", message);
  return error;
}

}  // namespace

bool FileSystemProviderMountFunction::RunImpl() {
  using extensions::api::file_system_provider::Mount::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // It's an error if the display name is empty.
  if (params->display_name.empty()) {
    base::ListValue* result = new base::ListValue();
    result->Append(new base::StringValue(""));
    result->Append(CreateError(kSecurityErrorName,
                               kEmptyNameErrorMessage));
    SetResult(result);
    return false;
  }

  chromeos::file_system_provider::Service* service =
      chromeos::file_system_provider::Service::Get(GetProfile());
  DCHECK(service);

  int file_system_id =
      service->RegisterFileSystem(extension_id(), params->display_name);

  // If the |file_system_id| is zero, then it means that registering the file
  // system failed.
  // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
  if (!file_system_id) {
    base::ListValue* result = new base::ListValue();
    result->Append(new base::FundamentalValue(0));
    result->Append(
        CreateError(kSecurityErrorName, kRegisteringFailedErrorMessage));
    SetResult(result);
    return false;
  }

  base::ListValue* result = new base::ListValue();
  result->Append(new base::FundamentalValue(file_system_id));
  // Don't append an error on success.

  SetResult(result);
  SendResponse(true);
  return true;
}

}  // namespace extensions
