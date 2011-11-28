// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "ui/views/controls/button/button.h"
#include "views/controls/link_listener.h"
#include "views/view.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/views/widget/native_widget_gtk.h"
#endif

class SkBitmap;

namespace views {
class ImageButton;
class ImageView;
class Label;
}

namespace chromeos {

class MessageBubbleDelegate : public BubbleDelegate {
 public:
  // Called when the user clicked on help link.
  // |index| identifies which link was clicked if there's more than one.
  virtual void OnLinkActivated(size_t index) = 0;
};

// MessageBubble is used to show error and info messages on OOBE screens.
class MessageBubble : public Bubble,
                      public views::ButtonListener,
                      public views::LinkListener {
 public:
  // Create and show bubble. position_relative_to must be in screen coordinates.
  // |links| is an optional vector of links texts.
  static MessageBubble* Show(views::Widget* parent,
                             const gfx::Rect& position_relative_to,
                             views::BubbleBorder::ArrowLocation arrow_location,
                             SkBitmap* image,
                             const std::wstring& text,
                             const std::wstring& help,
                             MessageBubbleDelegate* delegate);

  // Create and show bubble. position_relative_to must be in screen coordinates.
  // |links| is an optional vector of links texts.
  static MessageBubble* ShowWithLinks(
      views::Widget* parent,
      const gfx::Rect& position_relative_to,
      views::BubbleBorder::ArrowLocation arrow_location,
      SkBitmap* image,
      const std::wstring& text,
      const std::vector<std::wstring>& links,
      MessageBubbleDelegate* delegate);

  // Create and show bubble which does not grab pointer.  This creates
  // a TYPE_CHILD NativeWidgetGtk and |position_relative_to| must be in parent's
  // coordinates.
  static MessageBubble* ShowNoGrab(
      views::Widget* parent,
      const gfx::Rect& position_relative_to,
      views::BubbleBorder::ArrowLocation arrow_location,
      SkBitmap* image,
      const std::wstring& text,
      const std::wstring& help,
      MessageBubbleDelegate* delegate);

  // Overridden from NativeWidgetGtk/NativeWidgetViews.
  virtual void Close() OVERRIDE;

#if !defined(TOUCH_UI) && defined(TOOLKIT_USES_GTK)
  virtual gboolean OnButtonPress(GtkWidget* widget,
                                 GdkEventButton* event) OVERRIDE;
#endif

 protected:
  virtual ~MessageBubble();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

#if !defined(TOUCH_UI) && defined(TOOLKIT_USES_GTK)
  // Overridden from NativeWidgetGtk.
  virtual void OnActiveChanged() OVERRIDE;
  virtual void SetMouseCapture() OVERRIDE;
#endif

 private:
  MessageBubble(views::Widget::InitParams::Type type,
                views::Widget* parent,
                SkBitmap* image,
                const std::wstring& text,
                const std::vector<std::wstring>& links,
                bool grab_enabled,
                MessageBubbleDelegate* delegate);

  views::Widget* parent_;
  views::ImageView* icon_;
  views::Label* text_;
  views::ImageButton* close_button_;
  std::vector<views::Link*> help_links_;
  MessageBubbleDelegate* message_delegate_;
  bool grab_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MessageBubble);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
