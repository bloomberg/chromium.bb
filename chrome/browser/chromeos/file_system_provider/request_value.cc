// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/request_value.h"

namespace chromeos {
namespace file_system_provider {

RequestValue::RequestValue() {
}

RequestValue::~RequestValue() {
}

scoped_ptr<RequestValue> RequestValue::CreateForUnmountSuccess(
    scoped_ptr<extensions::api::file_system_provider_internal::
                   UnmountRequestedSuccess::Params> params) {
  scoped_ptr<RequestValue> result(new RequestValue);
  result->unmount_success_params_ = params.Pass();
  return result.Pass();
}

scoped_ptr<RequestValue> RequestValue::CreateForTesting(
    const std::string& params) {
  scoped_ptr<RequestValue> result(new RequestValue);
  result->testing_params_.reset(new std::string(params));
  return result.Pass();
}

}  // namespace file_system_provider
}  // namespace chromeos
