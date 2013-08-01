// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/recovery_private/recovery_private_api.h"

namespace extensions {

namespace recovery = api::recovery_private;

RecoveryPrivateWriteFromUrlFunction::RecoveryPrivateWriteFromUrlFunction() {
}

RecoveryPrivateWriteFromUrlFunction::~RecoveryPrivateWriteFromUrlFunction() {
}

bool RecoveryPrivateWriteFromUrlFunction::RunImpl() {
  scoped_ptr<recovery::WriteFromUrl::Params> params(
      recovery::WriteFromUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SendResponse(true);

  return true;
}

RecoveryPrivateWriteFromFileFunction::RecoveryPrivateWriteFromFileFunction() {
}

RecoveryPrivateWriteFromFileFunction::~RecoveryPrivateWriteFromFileFunction() {
}

bool RecoveryPrivateWriteFromFileFunction::RunImpl() {
  scoped_ptr<recovery::WriteFromFile::Params> params(
      recovery::WriteFromFile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SendResponse(true);

  return true;
}

RecoveryPrivateCancelWriteFunction::RecoveryPrivateCancelWriteFunction() {
}

RecoveryPrivateCancelWriteFunction::~RecoveryPrivateCancelWriteFunction() {
}

bool RecoveryPrivateCancelWriteFunction::RunImpl() {
  SendResponse(true);

  return true;
}

RecoveryPrivateDestroyPartitionsFunction::
    RecoveryPrivateDestroyPartitionsFunction() {
}

RecoveryPrivateDestroyPartitionsFunction::
    ~RecoveryPrivateDestroyPartitionsFunction() {
}

bool RecoveryPrivateDestroyPartitionsFunction::RunImpl() {
  scoped_ptr<recovery::DestroyPartitions::Params> params(
      recovery::DestroyPartitions::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SendResponse(true);

  return true;
}

}  // namespace extensions
