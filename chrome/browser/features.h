// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains features that need inclusion from multiple files. Features used from
// a single .cc file should be declared in the file's anonymous namespace.

#ifndef CHROME_BROWSER_FEATURES_H_
#define CHROME_BROWSER_FEATURES_H_

#include "base/feature_list.h"

namespace features {

extern const base::Feature kDesktopFastShutdown;

}  // namespace features

#endif  // CHROME_BROWSER_FEATURES_H_
