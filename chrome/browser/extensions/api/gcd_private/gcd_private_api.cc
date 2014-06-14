// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"

namespace extensions {

namespace {
base::LazyInstance<BrowserContextKeyedAPIFactory<GcdPrivateAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;
}

GcdPrivateAPI::GcdPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
}

GcdPrivateAPI::~GcdPrivateAPI() {
}

// static
BrowserContextKeyedAPIFactory<GcdPrivateAPI>*
GcdPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

bool GcdPrivateGetCloudDeviceListFunction::RunAsync() {
  results_.reset(new base::ListValue);
  results_->Append(new base::ListValue);

  SendResponse(true);
  return true;
}

}  // namespace extensions
