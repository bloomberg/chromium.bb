// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/icons/icons_api.h"

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<IconsAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<IconsAPI>* IconsAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

IconsAPI::IconsAPI(Profile* profile) {
  (new IconsHandler)->Register();
}

IconsAPI::~IconsAPI() {
}

}  // namespace extensions
