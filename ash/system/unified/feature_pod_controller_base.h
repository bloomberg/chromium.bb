// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_

namespace ash {

class FeaturePodButton;

// Base class for controllers of feature pod buttons.
// To add a new feature pod button, implement this class, and add to the list in
// UnifiedSystemTrayController::InitFeaturePods().
class FeaturePodControllerBase {
 public:
  virtual ~FeaturePodControllerBase() {}

  // Create the view. Subclasses instantiate FeaturePodButton.
  // The view will be onwed by views hierarchy. The view will be always deleted
  // after the controller is destructed (UnifiedSystemTrayBubble guarantees
  // this).
  virtual FeaturePodButton* CreateButton() = 0;

  // Called when the feature pod button is clicked.
  virtual void OnPressed() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_
