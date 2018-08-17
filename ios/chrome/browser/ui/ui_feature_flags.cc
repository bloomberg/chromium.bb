// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ui_feature_flags.h"

const base::Feature kRefreshPopupPresentation{
    "UIRefreshPopupPresentation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUIRefreshLocationBar{"UIRefreshLocationBar",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUIRefreshPhase1{"UIRefreshPhase1",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kInfobarsUIReboot{"InfobarsUIReboot",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kFirstResponderKeyWindow{"FirstResponderKeyWindow",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCopyImage{"CopyImage", base::FEATURE_DISABLED_BY_DEFAULT};
