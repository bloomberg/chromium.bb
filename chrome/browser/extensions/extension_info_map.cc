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

ExtensionInfoMap::ExtensionInfoMap() {
}

ExtensionInfoMap::~ExtensionInfoMap() {
}

void ExtensionInfoMap::AddExtension(const Extension* extension) {
  CheckOnValidThread();
  extensions_.Insert(extension);
  disabled_extensions_.Remove(extension->id());
}

void ExtensionInfoMap::RemoveExtension(const std::string& id,
    const UnloadedExtensionInfo::Reason reason) {
  CheckOnValidThread();
  const Extension* extension = extensions_.GetByID(id);
  if (extension) {
    if (reason == UnloadedExtensionInfo::DISABLE)
      disabled_extensions_.Insert(extension);
    extensions_.Remove(id);
  } else if (reason != UnloadedExtensionInfo::DISABLE) {
    // If the extension was uninstalled, make sure it's removed from the map of
    // disabled extensions.
    disabled_extensions_.Remove(id);
  } else {
    // NOTE: This can currently happen if we receive multiple unload
    // notifications, e.g. setting incognito-enabled state for a
    // disabled extension (e.g., via sync).  See
    // http://code.google.com/p/chromium/issues/detail?id=50582 .
    NOTREACHED() << id;
  }
}
