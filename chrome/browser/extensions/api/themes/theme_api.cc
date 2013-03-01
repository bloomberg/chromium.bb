// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/themes/theme_api.h"

#include "base/lazy_instance.h"
#include "chrome/common/extensions/api/themes/theme_handler.h"

namespace extensions {

ThemeAPI::ThemeAPI(Profile* profile) {
  (new extensions::ThemeHandler)->Register();
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
