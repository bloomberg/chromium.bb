// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/plugins/plugins_api.h"

#include "base/lazy_instance.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<PluginsAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<PluginsAPI>* PluginsAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

PluginsAPI::PluginsAPI(Profile* profile) {
  (new PluginsHandler)->Register();
}

PluginsAPI::~PluginsAPI() {
}

}  // namespace extensions
