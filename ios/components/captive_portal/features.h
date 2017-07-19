// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_CAPTIVE_PORTAL_FEATURES_H_
#define IOS_COMPONENTS_CAPTIVE_PORTAL_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace captive_portal {

#if defined(OS_IOS)
// Used to control the state of the Captive Portal Login feature.
extern const base::Feature kIosCaptivePortal;
#endif

}  // namespace captive_portal

#endif  // IOS_COMPONENTS_CAPTIVE_PORTAL_FEATURES_H_
