// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"

Extension* ExtensionFunction::GetExtension() {
  ExtensionsService* service = profile_->GetExtensionsService();
  DCHECK(service);
  return service->GetExtensionById(extension_id_, false);
}

void AsyncExtensionFunction::SetArgs(const ListValue* args) {
  DCHECK(!args_.get());  // Should only be called once.
  args_.reset(static_cast<ListValue*>(args->DeepCopy()));
}

const std::string AsyncExtensionFunction::GetResult() {
  std::string json;
  // Some functions might not need to return any results.
  if (result_.get())
    base::JSONWriter::Write(result_.get(), false, &json);
  return json;
}

void AsyncExtensionFunction::SendResponse(bool success) {
  if (!dispatcher())
    return;
  if (bad_message_) {
    dispatcher()->HandleBadMessage(this);
  } else {
    dispatcher()->SendResponse(this, success);
  }
}

bool AsyncExtensionFunction::HasOptionalArgument(size_t index) {
  Value* value;
  return args_->Get(index, &value) && !value->IsType(Value::TYPE_NULL);
}
