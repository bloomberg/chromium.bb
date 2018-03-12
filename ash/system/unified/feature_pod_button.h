// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_

#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace ash {

class FeaturePodControllerBase;

// ImageButon internally used in FeaturePodButton. Should not be used directly.
class FeaturePodIconButton : public views::ImageButton {
 public:
  explicit FeaturePodIconButton(views::ButtonListener* listener);
  ~FeaturePodIconButton() override;

  // Change the toggle state. See FeaturePodButton::SetToggled.
  void SetToggled(bool toggled);

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

  bool toggled() const { return toggled_; }

 private:
  // Ture if the button is currently toggled.
  bool toggled_ = false;

  DISALLOW_COPY_AND_ASSIGN(FeaturePodIconButton);
};

// A button in FeaturePodsView. These buttons are main entry points of features
// in UnifiedSystemTray. Each button has its icon, label, and sub-label placed
// vertically. They are also togglable and the background color indicates the
// current state.
// See the comment in FeaturePodsView for detail.
class FeaturePodButton : public views::View, public views::ButtonListener {
 public:
  explicit FeaturePodButton(FeaturePodControllerBase* controller);
  ~FeaturePodButton() override;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Set the text of label shown below the icon.
  void SetLabel(const base::string16& label);

  // Set the text of sub-label shown below the label.
  void SetSubLabel(const base::string16& sub_label);

  // Change the toggled state. If toggled, the background color of the circle
  // will change.
  void SetToggled(bool toggled);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  bool IsToggled() const { return icon_button_->toggled(); }

 protected:
  FeaturePodIconButton* icon_button() const { return icon_button_; }

 private:
  // Unowned.
  FeaturePodControllerBase* const controller_;

  // Owned by views hierarchy.
  FeaturePodIconButton* const icon_button_;
  views::Label* const label_;
  views::Label* sub_label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FeaturePodButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_
