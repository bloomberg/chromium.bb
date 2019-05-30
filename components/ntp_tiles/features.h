// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_FEATURES_H_
#define COMPONENTS_NTP_TILES_FEATURES_H_

namespace base {
struct Feature;
}  // namespace base

namespace ntp_tiles {

// If enabled, show a Google search shortcut on the NTP by default.
extern const base::Feature kDefaultSearchShortcut;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_FEATURES_H_
