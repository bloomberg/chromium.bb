// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/synthesize_browser_action_handler.h"

#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

SynthesizeBrowserActionHandler::SynthesizeBrowserActionHandler() {
}

SynthesizeBrowserActionHandler::~SynthesizeBrowserActionHandler() {
}

bool SynthesizeBrowserActionHandler::Parse(Extension* extension,
                                           base::string16* error) {
  if (!extensions::FeatureSwitch::extension_action_redesign()->IsEnabled())
    return true;  // Do nothing.

  if (extension->location() == Manifest::COMPONENT ||
      extension->location() == Manifest::EXTERNAL_COMPONENT)
    return true;  // Return no error (we're done).

  if (extension->manifest()->HasKey(manifest_keys::kSynthesizeBrowserAction))
    return false;  // This key is reserved, no extension should be using it.

  if (!extension->manifest()->HasKey(manifest_keys::kBrowserAction) &&
      !extension->manifest()->HasKey(manifest_keys::kPageAction))
    ActionInfo::SetBrowserActionInfo(extension, new ActionInfo());
  return true;
}

bool SynthesizeBrowserActionHandler::AlwaysParseForType(
    Manifest::Type type) const {
  return type == Manifest::TYPE_EXTENSION;
}

const std::vector<std::string> SynthesizeBrowserActionHandler::Keys() const {
  return SingleKey(manifest_keys::kSynthesizeBrowserAction);
}

}  // namespace extensions
