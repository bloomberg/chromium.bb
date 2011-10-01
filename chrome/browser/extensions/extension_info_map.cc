// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_info_map.h"

#include "chrome/common/extensions/extension.h"
#include "content/browser/browser_thread.h"

namespace {

static void CheckOnValidThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

}  // namespace


struct ExtensionInfoMap::ExtraData {
  // When the extension was installed.
  base::Time install_time;

  // True if the user has allowed this extension to run in incognito mode.
  bool incognito_enabled;

  ExtraData();
  ~ExtraData();
};

ExtensionInfoMap::ExtraData::ExtraData() : incognito_enabled(false) {
}

ExtensionInfoMap::ExtraData::~ExtraData() {
}


ExtensionInfoMap::ExtensionInfoMap() {
}

ExtensionInfoMap::~ExtensionInfoMap() {
}

void ExtensionInfoMap::AddExtension(const Extension* extension,
                                    base::Time install_time,
                                    bool incognito_enabled) {
  CheckOnValidThread();
  extensions_.Insert(extension);
  disabled_extensions_.Remove(extension->id());

  extra_data_[extension->id()].install_time = install_time;
  extra_data_[extension->id()].incognito_enabled = incognito_enabled;
}

void ExtensionInfoMap::RemoveExtension(const std::string& extension_id,
    const extension_misc::UnloadedExtensionReason reason) {
  CheckOnValidThread();
  const Extension* extension = extensions_.GetByID(extension_id);
  extra_data_.erase(extension_id);  // we don't care about disabled extra data
  if (extension) {
    if (reason == extension_misc::UNLOAD_REASON_DISABLE)
      disabled_extensions_.Insert(extension);
    extensions_.Remove(extension_id);
  } else if (reason != extension_misc::UNLOAD_REASON_DISABLE) {
    // If the extension was uninstalled, make sure it's removed from the map of
    // disabled extensions.
    disabled_extensions_.Remove(extension_id);
  } else {
    // NOTE: This can currently happen if we receive multiple unload
    // notifications, e.g. setting incognito-enabled state for a
    // disabled extension (e.g., via sync).  See
    // http://code.google.com/p/chromium/issues/detail?id=50582 .
    NOTREACHED() << extension_id;
  }
}

base::Time ExtensionInfoMap::GetInstallTime(
    const std::string& extension_id) const {
  ExtraDataMap::const_iterator iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    return iter->second.install_time;
  return base::Time();
}

bool ExtensionInfoMap::IsIncognitoEnabled(
    const std::string& extension_id) const {
  ExtraDataMap::const_iterator iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    return iter->second.incognito_enabled;
  return false;
}

bool ExtensionInfoMap::CanCrossIncognito(const Extension* extension) {
  // This is duplicated from ExtensionService :(.
  return IsIncognitoEnabled(extension->id()) &&
      !extension->incognito_split_mode();
}

// These are duplicated from ExtensionProcessManager :(.
void ExtensionInfoMap::BindingsEnabledForProcess(int host_id) {
  extension_bindings_process_ids_.insert(host_id);
}

void ExtensionInfoMap::BindingsDisabledForProcess(int host_id) {
  extension_bindings_process_ids_.erase(host_id);
}

bool ExtensionInfoMap::AreBindingsEnabledForProcess(int host_id) const {
  return extension_bindings_process_ids_.find(host_id) !=
      extension_bindings_process_ids_.end();
}
