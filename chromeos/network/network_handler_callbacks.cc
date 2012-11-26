// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_handler_callbacks.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"

namespace chromeos {
namespace network_handler {

const char kLogModule[] = "ShillError";

// These are names of fields in the error data dictionary for ErrorCallback.
const char kErrorName[] = "errorName";
const char kErrorMessage[] = "errorMessage";
const char kServicePath[] = "servicePath";

base::DictionaryValue* CreateErrorData(const std::string& service_path,
                                       const std::string& error_name,
                                       const std::string& error_message) {
  base::DictionaryValue* error_data(new base::DictionaryValue);
  error_data->SetString(kErrorName, error_name);
  error_data->SetString(kErrorMessage, error_message);
  if (!service_path.empty())
    error_data->SetString(kServicePath, service_path);
  return error_data;
}

void ShillErrorCallbackFunction(const std::string& module,
                                const std::string& path,
                                const ErrorCallback& error_callback,
                                const std::string& error_name,
                                const std::string& error_message) {
  std::string error = "Shill Error in " + module;
  if (!path.empty())
    error += " For " + path;
  error += ": " + error_name + " : " + error_message;
  LOG(ERROR) << error;
  network_event_log::AddEntry(kLogModule, module, error);
  if (error_callback.is_null())
    return;
  scoped_ptr<base::DictionaryValue> error_data(
      CreateErrorData(path, error_name, error_message));
  error_callback.Run(error_name, error_data.Pass());
}

} // namespace network_handler
}  // namespace chromeos
