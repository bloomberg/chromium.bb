// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_

namespace arc {

class ArcIntentHelperObserver {
 public:
  // Called when app's intent filter is updated.
  virtual void OnAppsUpdated() = 0;

 protected:
  virtual ~ArcIntentHelperObserver() = default;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_
