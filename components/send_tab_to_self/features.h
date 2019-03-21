// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_

#include "base/feature_list.h"

namespace send_tab_to_self {

// If this feature is enabled, we handle notifying users of sent tabs even if
// the sync feature is disabled.
extern const base::Feature kSendTabToSelfReceive;
}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
