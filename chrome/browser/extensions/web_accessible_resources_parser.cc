// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/web_accessible_resources_parser.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/web_accessible_resources_handler.h"

namespace extensions {

WebAccessibleResourcesParser::WebAccessibleResourcesParser(Profile* profile) {
  (new WebAccessibleResourcesHandler)->Register();
}

WebAccessibleResourcesParser::~WebAccessibleResourcesParser() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<WebAccessibleResourcesParser> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<WebAccessibleResourcesParser>*
    WebAccessibleResourcesParser::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
