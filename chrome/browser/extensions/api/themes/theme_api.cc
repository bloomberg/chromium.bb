// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/themes/theme_api.h"

#include "base/lazy_instance.h"
#include "chrome/common/extensions/api/themes/theme_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

ThemeAPI::ThemeAPI(Profile* profile) {
  // Register the ManifestHandler for parsing 'theme' manifest key.
  extensions::ManifestHandler::Register(
      extension_manifest_keys::kTheme,
      make_linked_ptr(new extensions::ThemeHandler));
}

ThemeAPI::~ThemeAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<ThemeAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<ThemeAPI>* ThemeAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
