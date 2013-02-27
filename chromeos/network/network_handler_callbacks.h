// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_
#define CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace network_handler {

// An error callback used by both the configuration handler and the state
// handler to receive error results from the API.
typedef base::Callback<
  void(const std::string& error_name,
       const scoped_ptr<base::DictionaryValue> error_data)> ErrorCallback;

typedef base::Callback<
  void(const std::string& service_path,
       const base::DictionaryValue& dictionary)> DictionaryResultCallback;

typedef base::Callback<
  void(const std::string& service_path)> StringResultCallback;

// Create a DictionaryValue for passing to ErrorCallback
CHROMEOS_EXPORT base::DictionaryValue* CreateErrorData(
    const std::string& service_path,
    const std::string& error_name,
    const std::string& error_message);

// Callback for Shill errors. |path| may be blank if not relevant.
// Logs an error and calls |error_callback| if not null.
void ShillErrorCallbackFunction(const std::string& module,
                                const std::string& path,
                                const ErrorCallback& error_callback,
                                const std::string& error_name,
                                const std::string& error_message);

}  // namespace network_handler
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_
