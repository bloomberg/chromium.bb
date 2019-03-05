// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"

#include <utility>

#include "base/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"

namespace extensions {

BookmarkAppRegistrar::BookmarkAppRegistrar(Profile* profile)
    : profile_(profile) {}

BookmarkAppRegistrar::~BookmarkAppRegistrar() = default;

void BookmarkAppRegistrar::Init(base::OnceClosure callback) {
  ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::AdaptCallbackForRepeating(std::move(callback)));
}

bool BookmarkAppRegistrar::IsInstalled(const web_app::AppId& app_id) const {
  return ExtensionRegistry::Get(profile_)->GetInstalledExtension(app_id) !=
         nullptr;
}

bool BookmarkAppRegistrar::WasExternalAppUninstalledByUser(
    const web_app::AppId& app_id) const {
  return ExtensionPrefs::Get(profile_)->IsExternalExtensionUninstalled(app_id);
}

}  // namespace extensions
