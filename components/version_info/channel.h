// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_INFO_CHANNEL_H_
#define COMPONENTS_VERSION_INFO_CHANNEL_H_

namespace version_info {

// The possible channels for an installation, from most fun to most stable.
enum class Channel { UNKNOWN = 0, CANARY, DEV, BETA, STABLE };

}  // namespace version_info

#endif  // COMPONENTS_VERSION_INFO_CHANNEL_H_
