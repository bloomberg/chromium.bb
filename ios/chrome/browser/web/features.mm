// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/features.h"

// Feature flag to control default value for mailto: URL rewriting feature.
// Change the default value here to enable or disable this feature.
const base::Feature kMailtoUrlRewriting{"MailtoUrlRewriting",
                                        base::FEATURE_ENABLED_BY_DEFAULT};
