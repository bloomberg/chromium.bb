// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/runtime_data.h"

#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

RuntimeData::RuntimeData(ExtensionRegistry* registry) : registry_(registry) {
  registry_->AddObserver(this);
}

RuntimeData::~RuntimeData() {
  registry_->RemoveObserver(this);
}

bool RuntimeData::IsBackgroundPageReady(const Extension* extension) const {
  if (!BackgroundInfo::HasPersistentBackgroundPage(extension))
    return true;
  return HasFlag(extension, BACKGROUND_PAGE_READY);
}

void RuntimeData::SetBackgroundPageReady(const Extension* extension,
                                         bool value) {
  SetFlag(extension, BACKGROUND_PAGE_READY, value);
}

bool RuntimeData::IsBeingUpgraded(const Extension* extension) const {
  return HasFlag(extension, BEING_UPGRADED);
}

void RuntimeData::SetBeingUpgraded(const Extension* extension, bool value) {
  SetFlag(extension, BEING_UPGRADED, value);
}

bool RuntimeData::HasUsedWebRequest(const Extension* extension) const {
  return HasFlag(extension, HAS_USED_WEBREQUEST);
}

void RuntimeData::SetHasUsedWebRequest(const Extension* extension, bool value) {
  SetFlag(extension, HAS_USED_WEBREQUEST, value);
}

bool RuntimeData::HasExtensionForTesting(const Extension* extension) const {
  return extension_flags_.find(extension->id()) != extension_flags_.end();
}

void RuntimeData::ClearAll() {
  extension_flags_.clear();
}

void RuntimeData::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  extension_flags_.erase(extension->id());
}

bool RuntimeData::HasFlag(const Extension* extension, RuntimeFlag flag) const {
  ExtensionFlagsMap::const_iterator it = extension_flags_.find(extension->id());
  if (it == extension_flags_.end())
    return false;
  return !!(it->second & flag);
}

void RuntimeData::SetFlag(const Extension* extension,
                          RuntimeFlag flag,
                          bool value) {
  if (value)
    extension_flags_[extension->id()] |= flag;
  else
    extension_flags_[extension->id()] &= ~flag;
}

}  // namespace extensions
