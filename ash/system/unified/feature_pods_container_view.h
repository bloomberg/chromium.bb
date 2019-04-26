// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// Container of feature pods buttons in the middle of UnifiedSystemTrayView.
// The container has number of buttons placed in 3x3 plane at regular distances.
// FeaturePodButtons implements these individual buttons.
// The container also implements collapsed state where the top 5 buttons are
// horizontally placed and others are hidden.
class ASH_EXPORT FeaturePodsContainerView : public views::View {
 public:
  explicit FeaturePodsContainerView(bool initially_expanded);
  ~FeaturePodsContainerView() override;

  // Change the expanded state. 0.0 if collapsed, and 1.0 if expanded.
  // Otherwise, it shows intermediate state. If collapsed, all the buttons are
  // horizontally placed.
  void SetExpandedAmount(double expanded_amount);

  // Get height of the view when |expanded_amount| is set to 1.0.
  int GetExpandedHeight() const;

  // Get the height of the view when |expanded_amount| is set to 0.0.
  int GetCollapsedHeight() const;

  // Save and restore keyboard focus of a child feature pod button. If no button
  // has focus or no focus is saved, these methods are no-op.
  void SaveFocus();
  void RestoreFocus();

  // Returns the number of children that prefer to be visible.
  int GetVisibleCount() const;

  // Overridden views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildVisibilityChanged(View* child) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void Layout() override;

 private:
  void UpdateChildVisibility();

  // Calculate the current position of the button from |visible_index| and
  // |expanded_amount_|.
  gfx::Point GetButtonPosition(int visible_index) const;

  // Update |collapsed_state_padding_|.
  void UpdateCollapsedSidePadding();

  // The last |expanded_amount| passed to SetExpandedAmount().
  double expanded_amount_;

  // Horizontal side padding in dip for collapsed state.
  int collapsed_side_padding_ = 0;

  // Used for preventing reentrancy issue in ChildVisibilityChanged. Should be
  // always false if FeaturePodsContainerView is not in the call stack.
  bool changing_visibility_ = false;

  // A button that had focus at the point SaveButtonFocus is called.
  views::View* focused_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FeaturePodsContainerView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_
