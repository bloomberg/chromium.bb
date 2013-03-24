// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/content_scripts_parser.h"

#include "base/lazy_instance.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<ContentScriptsParser> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<ContentScriptsParser>*
ContentScriptsParser::GetFactoryInstance() {
  return &g_factory.Get();
}

ContentScriptsParser::ContentScriptsParser(Profile* profile) {
  (new ContentScriptsHandler)->Register();
}

ContentScriptsParser::~ContentScriptsParser() {
}

}  // namespace extensions
