// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_NOTIFICATION_SWIPE_CONTROL_VIEW_H_
#define ASH_MESSAGE_CENTER_NOTIFICATION_SWIPE_CONTROL_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace ash {

// View containing 2 buttons that appears behind notification by swiping.
class ASH_EXPORT NotificationSwipeControlView : public views::View,
                                                public views::ButtonListener {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSettingsButtonPressed(const ui::Event& event) = 0;
    virtual void OnSnoozeButtonPressed(const ui::Event& event) = 0;
  };

  // Physical positions to show buttons in the swipe control. This is invariant
  // across RTL/LTR languages because buttons should be shown on one side which
  // is made uncovered by the overlapping view after user's swipe action.
  enum class ButtonPosition { RIGHT, LEFT };

  // String to be returned by GetClassName() method.
  static const char kViewClassName[];

  NotificationSwipeControlView();
  ~NotificationSwipeControlView() override;

  // views::View
  const char* GetClassName() const override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Change the visibility of the settings button.
  void ShowButtons(ButtonPosition button_position,
                   bool has_settings,
                   bool has_snooze);
  void HideButtons();
  void AddObserver(Observer* observer);

 private:
  // Change the visibility of the settings button. True to show, false to hide.
  void ShowSettingsButton(bool show);

  // Change the visibility of the snooze button. True to show, false to hide.
  void ShowSnoozeButton(bool show);

  // Owned by views hierarchy.
  views::ImageButton* settings_button_ = nullptr;
  views::ImageButton* snooze_button_ = nullptr;

  base::ObserverList<Observer,
                     false /* check_empty */,
                     false /* allow_reentrancy */>
      button_observers_;

  DISALLOW_COPY_AND_ASSIGN(NotificationSwipeControlView);
};

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_NOTIFICATION_SWIPE_CONTROL_VIEW_H_
