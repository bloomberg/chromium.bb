// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/web_intents_parser.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/web_intents_handler.h"

namespace extensions {

WebIntentsParser::WebIntentsParser(Profile* profile) {
  ManifestHandler::Register(extension_manifest_keys::kIntents,
                            new WebIntentsHandler);
}

WebIntentsParser::~WebIntentsParser() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<WebIntentsParser> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<WebIntentsParser>*
    WebIntentsParser::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
