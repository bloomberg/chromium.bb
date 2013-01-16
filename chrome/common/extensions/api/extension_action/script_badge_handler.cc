// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/script_badge_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/manifest_handler_helpers.h"

namespace errors = extension_manifest_errors;

namespace extensions {

ScriptBadgeHandler::ScriptBadgeHandler() {
}

ScriptBadgeHandler::~ScriptBadgeHandler() {
}

bool ScriptBadgeHandler::Parse(const base::Value* value,
                               Extension* extension,
                               string16* error) {
  scoped_ptr<ActionInfo> action_info(new ActionInfo);

  // So as to not confuse developers if they specify a script badge section
  // in the manifest, show a warning if the script badge declaration isn't
  // going to have any effect.
  if (!FeatureSwitch::script_badges()->IsEnabled()) {
    extension->AddInstallWarning(
        Extension::InstallWarning(Extension::InstallWarning::FORMAT_TEXT,
                                  errors::kScriptBadgeRequiresFlag));
  }

  const DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidScriptBadge);
    return false;
  }

  action_info =
      manifest_handler_helpers::LoadActionInfo(extension, dict, error);

  if (!action_info.get())
    return false;  // Failed to parse script badge definition.

  // Script badges always use their extension's title and icon so users can rely
  // on the visual appearance to know which extension is running.  This isn't
  // bulletproof since an malicious extension could use a different 16x16 icon
  // that matches the icon of a trusted extension, and users wouldn't be warned
  // during installation.

  if (!action_info->default_title.empty()) {
    extension->AddInstallWarning(
        Extension::InstallWarning(Extension::InstallWarning::FORMAT_TEXT,
                                  errors::kScriptBadgeTitleIgnored));
  }

  if (!action_info->default_icon.empty()) {
    extension->AddInstallWarning(
        Extension::InstallWarning(Extension::InstallWarning::FORMAT_TEXT,
                                  errors::kScriptBadgeIconIgnored));
  }

  SetActionInfoDefaults(extension, action_info.get());
  ActionInfo::SetScriptBadgeInfo(extension, action_info.release());
  return true;
}

bool ScriptBadgeHandler::HasNoKey(Extension* extension, string16* error) {
  scoped_ptr<ActionInfo> action_info(new ActionInfo);
  SetActionInfoDefaults(extension, action_info.get());
  ActionInfo::SetScriptBadgeInfo(extension, action_info.release());
  return true;
}

void ScriptBadgeHandler::SetActionInfoDefaults(const Extension* extension,
                                               ActionInfo* info) {
  info->default_title = extension->name();
  info->default_icon.Clear();
  for (size_t i = 0; i < extension_misc::kNumScriptBadgeIconSizes; ++i) {
    std::string path = extension->icons().Get(
        extension_misc::kScriptBadgeIconSizes[i],
        ExtensionIconSet::MATCH_BIGGER);
    if (!path.empty()) {
      info->default_icon.Add(
          extension_misc::kScriptBadgeIconSizes[i], path);
    }
  }
}

}  // namespace extensions
