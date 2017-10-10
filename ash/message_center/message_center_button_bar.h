// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_MESSAGE_CENTER_BUTTON_BAR_H_
#define ASH_MESSAGE_CENTER_MESSAGE_CENTER_BUTTON_BAR_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ToggleImageButton;
}  // namespace views

namespace message_center {
class MessageCenter;
class NotifierSettingsProvider;
}  // namespace message_center

namespace ash {

class MessageCenterView;

// MessageCenterButtonBar is the class that shows the content outside the main
// notification area - the label (or NotifierGroup switcher) and the buttons.
class MessageCenterButtonBar : public views::View,
                               public views::ButtonListener {
 public:
  MessageCenterButtonBar(
      MessageCenterView* message_center_view,
      message_center::MessageCenter* message_center,
      message_center::NotifierSettingsProvider* notifier_settings_provider,
      bool settings_initially_visible,
      const base::string16& title);
  ~MessageCenterButtonBar() override;

  void SetQuietModeState(bool is_quiet_mode);

  // Overridden from views::View:
  void ChildVisibilityChanged(views::View* child) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Enables or disables all of the buttons in the center.  This is used to
  // prevent user clicks during the close-all animation.
  void SetSettingsAndQuietModeButtonsEnabled(bool enabled);

  // Sometimes we shouldn't see the close-all button.
  void SetCloseAllButtonEnabled(bool enabled);

  ASH_EXPORT views::Button* GetCloseAllButtonForTest() const;
  ASH_EXPORT views::Button* GetSettingsButtonForTest() const;
  ASH_EXPORT views::Button* GetQuietModeButtonForTest() const;

  // Sometimes we shouldn't see the back arrow (not in settings).
  void SetBackArrowVisible(bool visible);

  // Update the label of the title.
  void SetTitle(const base::string16& title);

  void SetButtonsVisible(bool visible);

 private:
  // Updates the layout manager which can have differing configuration
  // depending on the visibility of different parts of the button bar.
  void ViewVisibilityChanged();

  MessageCenterView* message_center_view() const {
    return message_center_view_;
  }
  message_center::MessageCenter* message_center() const {
    return message_center_;
  }

  MessageCenterView* message_center_view_;
  message_center::MessageCenter* message_center_;

  // Sub-views of the button bar.
  views::ToggleImageButton* title_arrow_;
  views::Label* notification_label_;
  views::View* button_container_;
  views::ImageButton* close_all_button_;
  views::ImageButton* settings_button_;
  views::ToggleImageButton* quiet_mode_button_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterButtonBar);
};

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_MESSAGE_CENTER_BUTTON_BAR_H_
