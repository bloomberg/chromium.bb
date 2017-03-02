// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace content {

// Note that this enum is used to back an UMA histogram, so it should be
// treated as append-only.
enum UmaNavigationType {
  NAVIGATION_TYPE_NONE,
  FORWARD_TOUCHPAD,
  BACK_TOUCHPAD,
  FORWARD_TOUCHSCREEN,
  BACK_TOUCHSCREEN,
  NAVIGATION_TYPE_COUNT,
};

}  // namespace content
