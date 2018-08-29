// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NUX_CONSTANTS_H_
#define COMPONENTS_NUX_CONSTANTS_H_

namespace base {
struct Feature;
}  // namespace base

namespace nux {

extern const base::Feature kNuxEmailFeature;
extern const base::Feature kNuxGoogleAppsFeature;
extern const char kNuxEmailUrl[];
extern const char kNuxGoogleAppsUrl[];

}  // namespace nux

#endif  // COMPONENTS_NUX_CONSTANTS_H_
