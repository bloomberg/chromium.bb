// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nux/email/constants.h"

#include "base/feature_list.h"

namespace nux {

extern const base::Feature kNuxEmailFeature{"NuxEmail",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

extern const char kNuxEmailUrl[] = "chrome://welcome/email";

}  // namespace nux