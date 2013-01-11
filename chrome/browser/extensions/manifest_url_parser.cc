// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_url_parser.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"

namespace extensions {

ManifestURLParser::ManifestURLParser(Profile* profile) {
  ManifestHandler::Register(extension_manifest_keys::kDevToolsPage,
                            new DevToolsPageHandler);
  ManifestHandler::Register(extension_manifest_keys::kHomepageURL,
                            new HomepageURLHandler);
}

ManifestURLParser::~ManifestURLParser() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<ManifestURLParser> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<ManifestURLParser>*
    ManifestURLParser::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
