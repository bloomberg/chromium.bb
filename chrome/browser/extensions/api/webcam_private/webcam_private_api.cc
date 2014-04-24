// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webcam_private/webcam_private_api.h"

#include "chrome/common/extensions/api/webcam_private.h"

namespace extensions {

WebcamPrivateSetFunction::WebcamPrivateSetFunction() {
}

WebcamPrivateSetFunction::~WebcamPrivateSetFunction() {
}

bool WebcamPrivateSetFunction::RunImpl() {
  // Get parameters
  scoped_ptr<api::webcam_private::Set::Params> params(
      api::webcam_private::Set::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(zork): Send the value to the webcam.

  return true;
}

WebcamPrivateGetFunction::WebcamPrivateGetFunction() {
}

WebcamPrivateGetFunction::~WebcamPrivateGetFunction() {
}

bool WebcamPrivateGetFunction::RunImpl() {
  // Get parameters
  scoped_ptr<api::webcam_private::Get::Params> params(
      api::webcam_private::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(zork): Get the value from the webcam.

  return false;
}

WebcamPrivateResetFunction::WebcamPrivateResetFunction() {
}

WebcamPrivateResetFunction::~WebcamPrivateResetFunction() {
}

bool WebcamPrivateResetFunction::RunImpl() {
  // Get parameters
  scoped_ptr<api::webcam_private::Reset::Params> params(
      api::webcam_private::Reset::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(zork): Reset the webcam state.

  return true;
}

}  // namespace extensions
