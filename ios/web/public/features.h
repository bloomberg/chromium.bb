// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FEATURES_H_
#define IOS_WEB_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace web {
namespace features {

// Used to enable the WKBackForwardList based navigation manager.
extern const base::Feature kSlimNavigationManager;

// Used to enable PassKit download on top of ios/web Download API.
extern const base::Feature kNewPassKitDownload;

// Used to enable new Download Manager UI and backend.
extern const base::Feature kNewFileDownload;

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_PUBLIC_FEATURES_H_
