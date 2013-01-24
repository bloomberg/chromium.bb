// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace extensions {

namespace {

// Manifest keys.
const char kKeyword[] = "keyword";

}  // namespace

// static
const std::string& OmniboxInfo::GetKeyword(const Extension* extension) {
  OmniboxInfo* info = static_cast<OmniboxInfo*>(
      extension->GetManifestData(extension_manifest_keys::kOmnibox));
  return info ? info->keyword : EmptyString();
}

// static
bool OmniboxInfo::IsVerboseInstallMessage(const Extension* extension) {
  return !GetKeyword(extension).empty() ||
      extension->browser_action_info() ||
      (extension->page_action_info() &&
       (CommandsInfo::GetPageActionCommand(extension) ||
        !extension->page_action_info()->default_icon.empty()));
}

OmniboxHandler::OmniboxHandler() {
}

OmniboxHandler::~OmniboxHandler() {
}

bool OmniboxHandler::Parse(const base::Value* value,
                           Extension* extension,
                           string16* error) {
  scoped_ptr<OmniboxInfo> info(new OmniboxInfo);
  const DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict) ||
      !dict->GetString(kKeyword, &info->keyword) ||
      info->keyword.empty()) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidOmniboxKeyword);
    return false;
  }
  extension->SetManifestData(extension_manifest_keys::kOmnibox, info.release());
  return true;
}

}  // namespace extensions
