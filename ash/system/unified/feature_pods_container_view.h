// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// Container of feature pods buttons in the middle of UnifiedSystemTrayView.
// The container has number of buttons placed in plane at regular distances such
// as 3x2. FeaturePodButtons implements these individual buttons.
// The container will also implement collapsed state where all the buttons are
// horizontally placed.
class ASH_EXPORT FeaturePodsContainerView : public views::View {
 public:
  FeaturePodsContainerView();
  ~FeaturePodsContainerView() override;

  // Change the expanded state. If collapsed, all the buttons are horizontally
  // placed.
  void SetExpanded(bool expanded);

  // Overridden views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildVisibilityChanged(View* child) override;
  void Layout() override;

 private:
  void LayoutExpanded();
  void LayoutCollapsed();
  void UpdateChildVisibility();

  int CountVisibleChildren() const;

  bool expanded_ = true;

  bool changing_visibility_ = false;

  DISALLOW_COPY_AND_ASSIGN(FeaturePodsContainerView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_
