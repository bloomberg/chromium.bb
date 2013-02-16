// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/csp_parser.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/csp_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

CSPParser::CSPParser(Profile* profile) {
  ManifestHandler::Register(
      extension_manifest_keys::kContentSecurityPolicy,
      make_linked_ptr(new CSPHandler(false))); // not platform app.
  ManifestHandler::Register(
      extension_manifest_keys::kPlatformAppContentSecurityPolicy,
      make_linked_ptr(new CSPHandler(true))); // platform app.
}

CSPParser::~CSPParser() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<CSPParser> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<CSPParser>* CSPParser::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
