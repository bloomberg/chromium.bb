// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"

namespace extensions {

// Static member intialization.
template<>
base::LazyInstance<scoped_refptr<SystemInfoProvider<DisplayInfo> > >
  SystemInfoProvider<DisplayInfo>::provider_ = LAZY_INSTANCE_INITIALIZER;

const DisplayInfo& DisplayInfoProvider::display_info() const {
  return info_;
}

// static
DisplayInfoProvider* DisplayInfoProvider::GetProvider() {
  return DisplayInfoProvider::GetInstance<DisplayInfoProvider>();
}

}  // namespace extensions

