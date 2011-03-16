// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
#pragma once

#include "chrome/browser/ui/views/info_bubble.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"

class SkBitmap;

namespace views {
class ImageButton;
class ImageView;
class Label;
}

namespace chromeos {

class MessageBubbleDelegate : public InfoBubbleDelegate {
 public:
  // Called when the user clicked on help link.
  virtual void OnHelpLinkActivated() = 0;
};

// MessageBubble is used to show error and info messages on OOBE screens.
class MessageBubble : public InfoBubble,
                      public views::ButtonListener,
                      public views::LinkController {
 public:
  // Create and show bubble. position_relative_to must be in screen coordinates.
  static MessageBubble* Show(views::Widget* parent,
                             const gfx::Rect& position_relative_to,
                             BubbleBorder::ArrowLocation arrow_location,
                             SkBitmap* image,
                             const std::wstring& text,
                             const std::wstring& help,
                             MessageBubbleDelegate* delegate);

  // Create and show bubble which does not grab pointer.  This creates
  // a TYPE_CHILD WidgetGtk and |position_relative_to| must be in parent's
  // coordinates.
  static MessageBubble* ShowNoGrab(views::Widget* parent,
                                   const gfx::Rect& position_relative_to,
                                   BubbleBorder::ArrowLocation arrow_location,
                                   SkBitmap* image,
                                   const std::wstring& text,
                                   const std::wstring& help,
                                   MessageBubbleDelegate* delegate);

  // Overridden from WidgetGtk.
  virtual void Close() OVERRIDE;

  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
    WidgetGtk::OnButtonPress(widget, event);
    // Never propagate event to parent.
    return true;
  }

 protected:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from WidgetGtk.
  virtual void IsActiveChanged();
  virtual void SetNativeCapture();

 private:
  MessageBubble(views::WidgetGtk::Type type,
                views::Widget* parent,
                SkBitmap* image,
                const std::wstring& text,
                const std::wstring& help,
                bool grab_enabled,
                MessageBubbleDelegate* delegate);

  views::Widget* parent_;
  views::ImageView* icon_;
  views::Label* text_;
  views::ImageButton* close_button_;
  views::Link* help_link_;
  MessageBubbleDelegate* message_delegate_;
  bool grab_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MessageBubble);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
