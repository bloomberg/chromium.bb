// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POPUP_MESSAGE_H_
#define ASH_POPUP_MESSAGE_H_

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class Widget;
}

namespace ash {

// PopupMessage shows a message to the user. Since the user is not able to
// dismiss it, the calling code needs to explictly close and destroy it.
class ASH_EXPORT PopupMessage {
 public:
  enum IconType {
    ICON_WARNING,
    ICON_NONE
  };

  // Creates a message pointing towards |anchor| with the requested
  // |arrow_orientation|. The message contains an optional |caption| which is
  // drawn in bold and an optional |message| together with an optional icon of
  // shape |message_type|. If a component in |size_override| is not 0 the value
  // is the used as output size. If |arrow_offset| is not 0, the number is the
  // arrow offset in pixels from the border.
  //
  // Here is the layout (arrow given as TOP_LEFT):
  //                    |--------|
  //                    | Anchor |
  //                    |--------|
  //       |-arrow_offset---^
  //       +-------------------------------------------------+
  //      -|                                                 |-
  //  icon |  [!]  Caption in bold which can be multi line   | caption_label
  //      -|                                                 |-
  //       |       Message text which can be multi line      | message_label
  //       |       as well.                                  |
  //       |                                                 |-
  //       +-------------------------------------------------+
  PopupMessage(const base::string16& caption,
               const base::string16& message,
               IconType message_type,
               views::View* anchor,
               views::BubbleBorder::Arrow arrow,
               const gfx::Size& size_override,
               int arrow_offset);
  // If the message was not explicitly closed before, it closes the message
  // without animation.
  virtual ~PopupMessage();

  // Closes the message with a fade out animation.
  void Close();

 private:
  FRIEND_TEST_ALL_PREFIXES(PopupMessageTest, Layout);

  class MessageBubble;

  static const int kCaptionLabelID;
  static const int kMessageLabelID;

  void CancelHidingAnimation();

  MessageBubble* view_;
  views::Widget* widget_;

  base::string16 caption_;
  base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(PopupMessage);
};

}  // namespace ash

#endif  // ASH_POPUP_MESSAGE_H_
