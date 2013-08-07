// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"

namespace extensions {

DisplayInfoProvider::DisplayInfoProvider() {
}

DisplayInfoProvider::~DisplayInfoProvider() {
}

// Static member intialization.
base::LazyInstance<scoped_refptr<DisplayInfoProvider > >
    DisplayInfoProvider::provider_ = LAZY_INSTANCE_INITIALIZER;

const DisplayInfo& DisplayInfoProvider::display_info() const {
  return info_;
}

void DisplayInfoProvider::InitializeForTesting(
    scoped_refptr<DisplayInfoProvider> provider) {
  DCHECK(provider.get() != NULL);
  provider_.Get() = provider;
}

// static
DisplayInfoProvider* DisplayInfoProvider::Get() {
  if (provider_.Get().get() == NULL)
    provider_.Get() = new DisplayInfoProvider();
  return provider_.Get();
}

}  // namespace extensions
