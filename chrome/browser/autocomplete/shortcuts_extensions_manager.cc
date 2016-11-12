// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_extensions_manager.h"

#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#endif

ShortcutsExtensionsManager::ShortcutsExtensionsManager(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  notification_registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
      content::Source<Profile>(profile_));
#endif
}

ShortcutsExtensionsManager::~ShortcutsExtensionsManager() {}

void ShortcutsExtensionsManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED, type);
  scoped_refptr<ShortcutsBackend> shortcuts_backend =
      ShortcutsBackendFactory::GetForProfileIfExists(profile_);
  if (!shortcuts_backend)
    return;

  // When an extension is unloaded, we want to remove any Shortcuts associated
  // with it.
  shortcuts_backend->DeleteShortcutsBeginningWithURL(
      content::Details<extensions::UnloadedExtensionInfo>(details)
          ->extension->url());
#endif
}
