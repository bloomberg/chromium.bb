// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/page_launcher/page_launcher_handler.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"

namespace keys = extension_manifest_keys;

namespace extensions {

PageLauncherHandler::PageLauncherHandler() {
}

PageLauncherHandler::~PageLauncherHandler() {
}

bool PageLauncherHandler::Parse(Extension* extension, string16* error) {
  DCHECK(extension->manifest()->HasKey(keys::kPageLauncher));

  const base::DictionaryValue* dict;
  if (!extension->manifest()->GetDictionary(keys::kPageLauncher, &dict)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidPageLauncher);
    return false;
  }

  scoped_ptr<ActionInfo> action_info = ActionInfo::Load(extension, dict, error);
  if (!action_info)
    return false;

  ActionInfo::SetPageLauncherInfo(extension, action_info.release());
  return true;
}

const std::vector<std::string> PageLauncherHandler::Keys() const {
  return SingleKey(keys::kPageLauncher);
}

}  // namespace extensions
