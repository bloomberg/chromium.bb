// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/action_info.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace extensions {

namespace {

// The manifest data container for the ActionInfos for BrowserActions and
// ScriptBadges.
struct ActionInfoData : public Extension::ManifestData {
  explicit ActionInfoData(ActionInfo* action_info);
  virtual ~ActionInfoData();

  // The action associated with the BrowserAction or ScriptBadge.
  // This is never NULL for ScriptBadge.
  scoped_ptr<ActionInfo> action_info;
};

ActionInfoData::ActionInfoData(ActionInfo* info) : action_info(info) {
}

ActionInfoData::~ActionInfoData() {
}

static const ActionInfo* GetActionInfo(const Extension* extension,
                                       const std::string& key) {
  ActionInfoData* data = static_cast<ActionInfoData*>(
      extension->GetManifestData(key));
  return data ? data->action_info.get() : NULL;
}

}  // namespace

ActionInfo::ActionInfo() {
}

ActionInfo::~ActionInfo() {
}

// static
const ActionInfo* ActionInfo::GetBrowserActionInfo(const Extension* extension) {
  return GetActionInfo(extension, extension_manifest_keys::kBrowserAction);
}

const ActionInfo* ActionInfo::GetPageActionInfo(const Extension* extension) {
  return GetActionInfo(extension, extension_manifest_keys::kPageAction);
}

// static
const ActionInfo* ActionInfo::GetScriptBadgeInfo(const Extension* extension) {
  return GetActionInfo(extension, extension_manifest_keys::kScriptBadge);
}

// static
const ActionInfo* ActionInfo::GetPageLauncherInfo(const Extension* extension) {
  return GetActionInfo(extension, extension_manifest_keys::kPageLauncher);
}

// static
void ActionInfo::SetBrowserActionInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(extension_manifest_keys::kBrowserAction,
                             new ActionInfoData(info));
}

// static
void ActionInfo::SetPageActionInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(extension_manifest_keys::kPageAction,
                             new ActionInfoData(info));
}

// static
void ActionInfo::SetScriptBadgeInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(extension_manifest_keys::kScriptBadge,
                             new ActionInfoData(info));
}

// static
void ActionInfo::SetPageLauncherInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(extension_manifest_keys::kPageLauncher,
                             new ActionInfoData(info));
}

// static
bool ActionInfo::IsVerboseInstallMessage(const Extension* extension) {
  const ActionInfo* page_action_info = GetPageActionInfo(extension);
  return page_action_info &&
      (CommandsInfo::GetPageActionCommand(extension) ||
       !page_action_info->default_icon.empty());
}

}  // namespace extensions
