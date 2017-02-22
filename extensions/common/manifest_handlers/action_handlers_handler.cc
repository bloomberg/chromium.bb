// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/action_handlers_handler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace app_runtime = api::app_runtime;
namespace errors = manifest_errors;
namespace keys = manifest_keys;

// static
bool ActionHandlersInfo::HasActionHandler(
    const Extension* extension,
    api::app_runtime::ActionType action_type) {
  ActionHandlersInfo* info = static_cast<ActionHandlersInfo*>(
      extension->GetManifestData(keys::kActionHandlers));
  return info && info->action_handlers.count(action_type) > 0;
}

ActionHandlersInfo::ActionHandlersInfo() = default;

ActionHandlersInfo::~ActionHandlersInfo() = default;

ActionHandlersHandler::ActionHandlersHandler() = default;

ActionHandlersHandler::~ActionHandlersHandler() = default;

bool ActionHandlersHandler::Parse(Extension* extension, base::string16* error) {
  const base::ListValue* entries = nullptr;
  if (!extension->manifest()->GetList(keys::kActionHandlers, &entries)) {
    *error = base::ASCIIToUTF16(errors::kInvalidActionHandlersType);
    return false;
  }

  auto info = base::MakeUnique<ActionHandlersInfo>();
  for (const std::unique_ptr<base::Value>& wrapped_value : *entries) {
    std::string value;
    if (!wrapped_value->GetAsString(&value)) {
      *error = base::ASCIIToUTF16(errors::kInvalidActionHandlersType);
      return false;
    }

    app_runtime::ActionType action_type = app_runtime::ParseActionType(value);
    if (action_type == app_runtime::ACTION_TYPE_NONE) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidActionHandlersActionType, value);
      return false;
    }

    info->action_handlers.insert(action_type);
  }

  extension->SetManifestData(keys::kActionHandlers, std::move(info));
  return true;
}

const std::vector<std::string> ActionHandlersHandler::Keys() const {
  return SingleKey(keys::kActionHandlers);
}

}  // namespace extensions
