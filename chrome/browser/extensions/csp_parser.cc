// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/csp_parser.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/csp_handler.h"
#include "chrome/common/extensions/manifest_handlers/sandboxed_page_info.h"

namespace extensions {

CSPParser::CSPParser(Profile* profile) {
  (new CSPHandler(false))->Register();  // not platform app.
  (new CSPHandler(true))->Register();  // platform app.
  (new SandboxedPageHandler)->Register();
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
