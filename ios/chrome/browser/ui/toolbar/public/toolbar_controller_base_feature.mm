// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"

const base::Feature kSafeAreaCompatibleToolbar{
    "SafeAreaCompatibleToolbar", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kPropertyAnimationsToolbar{
    "PropertyAnimationsToolbar", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kToolbarSnapshotAnimation{"ToolbarSnapshotAnimation",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
