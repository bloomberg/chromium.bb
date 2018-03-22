// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_

#include "ui/views/view.h"

namespace ash {

class FeaturePodButton;
class FeaturePodsContainerView;
class TopShortcutsView;
class UnifiedSystemInfoView;
class UnifiedSystemTrayController;

// View class of the main bubble in UnifiedSystemTray.
class UnifiedSystemTrayView : public views::View {
 public:
  explicit UnifiedSystemTrayView(UnifiedSystemTrayController* controller);
  ~UnifiedSystemTrayView() override;

  // Add feature pod button to |feature_pods_|.
  void AddFeaturePodButton(FeaturePodButton* button);

  // Add slider view.
  void AddSliderView(views::View* slider_view);

  // Change the expanded state.
  void SetExpanded(bool expanded);

 private:
  // Unowned.
  UnifiedSystemTrayController* controller_;

  // Owned by views hierarchy.
  TopShortcutsView* top_shortcuts_view_;
  FeaturePodsContainerView* feature_pods_container_;
  views::View* sliders_container_;
  UnifiedSystemInfoView* system_info_view_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_
