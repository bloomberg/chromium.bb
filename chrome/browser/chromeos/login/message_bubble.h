// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class SkBitmap;

namespace views {
class ImageButton;
}

namespace chromeos {

class MessageBubbleLinkListener {
 public:
  // Called when the user clicks on help link.
  // |index| identifies which link was clicked in case there's more than one.
  virtual void OnLinkActivated(size_t index) = 0;
};

// MessageBubble is used to show error and info messages on OOBE screens.
class MessageBubble : public views::BubbleDelegateView,
                      public views::ButtonListener,
                      public views::LinkListener {
 public:
  MessageBubble(views::View* anchor_view,
                views::BubbleBorder::ArrowLocation arrow_location,
                SkBitmap* image,
                const string16& text,
                const std::vector<string16>& links);

  virtual ~MessageBubble();

  void set_link_listener(MessageBubbleLinkListener* link_listener) {
    link_listener_ = link_listener;
  }

 protected:

  // Overridden from views::BubbleDelegateView:
  virtual void Init() OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  SkBitmap* image_;
  string16 text_;
  views::ImageButton* close_button_;
  std::vector<views::Link*> help_links_;
  MessageBubbleLinkListener* link_listener_;

  DISALLOW_COPY_AND_ASSIGN(MessageBubble);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MESSAGE_BUBBLE_H_
