// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/recovery_private/recovery_operation_manager.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_private_api.h"

namespace recovery_api = extensions::api::recovery_private;

namespace extensions {

const char kInvalidUrl[] = "Invalid URL provided.";

RecoveryPrivateWriteFromUrlFunction::RecoveryPrivateWriteFromUrlFunction() {
}

RecoveryPrivateWriteFromUrlFunction::~RecoveryPrivateWriteFromUrlFunction() {
}

bool RecoveryPrivateWriteFromUrlFunction::RunImpl() {
  scoped_ptr<recovery_api::WriteFromUrl::Params> params(
      recovery_api::WriteFromUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool save_image_as_download = false;

  if (params->options->save_as_download.get()) {
    save_image_as_download = true;
  }

  GURL url(params->image_url);

  if (!url.is_valid()) {
    error_ = kInvalidUrl;
    return false;
  }

  recovery::RecoveryOperationManager::Get(profile())->StartWriteFromUrl(
      extension_id(),
      url,
      params->options->image_hash.Pass(),
      save_image_as_download,
      params->storage_unit_id,
      base::Bind(&RecoveryPrivateWriteFromUrlFunction::OnWriteStarted, this));

  return true;
}

void RecoveryPrivateWriteFromUrlFunction::OnWriteStarted(bool success) {
  SendResponse(success);
}

RecoveryPrivateWriteFromFileFunction::RecoveryPrivateWriteFromFileFunction() {
}

RecoveryPrivateWriteFromFileFunction::~RecoveryPrivateWriteFromFileFunction() {
}

bool RecoveryPrivateWriteFromFileFunction::RunImpl() {
  scoped_ptr<recovery_api::WriteFromFile::Params> params(
      recovery_api::WriteFromFile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  recovery::RecoveryOperationManager::Get(profile())->StartWriteFromFile(
      extension_id(),
      params->storage_unit_id,
      base::Bind(&RecoveryPrivateWriteFromFileFunction::OnWriteStarted, this));

  return true;
}

void RecoveryPrivateWriteFromFileFunction::OnWriteStarted(bool success) {
  SendResponse(success);
}

RecoveryPrivateCancelWriteFunction::RecoveryPrivateCancelWriteFunction() {
}

RecoveryPrivateCancelWriteFunction::~RecoveryPrivateCancelWriteFunction() {
}

bool RecoveryPrivateCancelWriteFunction::RunImpl() {
  recovery::RecoveryOperationManager::Get(profile())->CancelWrite(
      extension_id(),
      base::Bind(&RecoveryPrivateCancelWriteFunction::OnWriteCancelled, this));

  return true;
}

void RecoveryPrivateCancelWriteFunction::OnWriteCancelled(bool success) {
  SendResponse(success);
}

RecoveryPrivateDestroyPartitionsFunction::
    RecoveryPrivateDestroyPartitionsFunction() {
}

RecoveryPrivateDestroyPartitionsFunction::
    ~RecoveryPrivateDestroyPartitionsFunction() {
}

bool RecoveryPrivateDestroyPartitionsFunction::RunImpl() {
  scoped_ptr<recovery_api::DestroyPartitions::Params> params(
      recovery_api::DestroyPartitions::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SendResponse(true);

  return true;
}

}  // namespace extensions
