// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_features.h"

namespace syncer {

// The feature controls the filling of the unencrypted metadata field for the
// passwords specifics https://crbug.com/638963.
const base::Feature kFillPasswordMetadata{"PasswordMetadataFilling",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace syncer
