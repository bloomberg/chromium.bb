// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include "base/basictypes.h"
#include "extensions/common/extensions_client.h"

namespace extensions {

FeatureProvider* FeatureProvider::GetByName(const std::string& name) {
  return ExtensionsClient::Get()->GetFeatureProviderByName(name);
}

}  // namespace extensions
