// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"

namespace extensions {

SystemIndicatorHandler::SystemIndicatorHandler() {
}

SystemIndicatorHandler::~SystemIndicatorHandler() {
}

bool SystemIndicatorHandler::Parse(Extension* extension, string16* error) {
  const DictionaryValue* system_indicator_value = NULL;
  if (!extension->manifest()->GetDictionary(
          extension_manifest_keys::kSystemIndicator, &system_indicator_value)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidSystemIndicator);
    return false;
  }

  scoped_ptr<ActionInfo> action_info = ActionInfo::Load(
      extension, system_indicator_value, error);

  if (!action_info.get())
    return false;

  // Because the manifest was successfully parsed, auto-grant the permission.
  // TODO(dewittj) Add this for all extension action APIs.
  extension->initial_api_permissions()->insert(APIPermission::kSystemIndicator);

  ActionInfo::SetSystemIndicatorInfo(extension, action_info.release());
  return true;
}

const std::vector<std::string> SystemIndicatorHandler::Keys() const {
  return SingleKey(extension_manifest_keys::kSystemIndicator);
}

}  // namespace extensions
