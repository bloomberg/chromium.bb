// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/action_info.h"

#include "base/memory/scoped_ptr.h"
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

}  // namespace

ActionInfo::ActionInfo() {
}

ActionInfo::~ActionInfo() {
}

// static
const ActionInfo* ActionInfo::GetScriptBadgeInfo(const Extension* extension) {
  ActionInfoData* data = static_cast<ActionInfoData*>(
      extension->GetManifestData(extension_manifest_keys::kScriptBadge));
  return data ? data->action_info.get() : NULL;
}

// static
void ActionInfo::SetScriptBadgeInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(extension_manifest_keys::kScriptBadge,
                             new ActionInfoData(info));
}

}  // namespace extensions
