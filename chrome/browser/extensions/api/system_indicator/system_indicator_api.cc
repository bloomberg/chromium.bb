// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_indicator/system_indicator_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"

namespace extensions {

SystemIndicatorAPI::SystemIndicatorAPI(Profile* profile) {
  (new SystemIndicatorHandler)->Register();
}

SystemIndicatorAPI::~SystemIndicatorAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<SystemIndicatorAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<SystemIndicatorAPI>*
SystemIndicatorAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
