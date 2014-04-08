// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"

class ManagePasswordsIconView;

namespace content {
class WebContents;
}

namespace views {
class BlueButton;
class LabelButton;
class GridLayout;
}

class ManagePasswordsBubbleView : public views::BubbleDelegateView,
                                  public views::ButtonListener,
                                  public views::ComboboxListener,
                                  public views::LinkListener {
 public:
  enum FieldType { USERNAME_FIELD, PASSWORD_FIELD };

  enum BubbleDisplayReason { AUTOMATIC = 0, USER_ACTION, NUM_DISPLAY_REASONS };

  enum BubbleDismissalReason {
    BUBBLE_LOST_FOCUS = 0,
    CLICKED_SAVE,
    CLICKED_NOPE,
    CLICKED_NEVER,
    CLICKED_MANAGE,
    CLICKED_DONE,
    NUM_DISMISSAL_REASONS,

    // If we add the omnibox icon _without_ intending to display the bubble,
    // we actually call Close() after creating the bubble view. We don't want
    // that to count in the metrics, so we need this placeholder value.
    NOT_DISPLAYED
  };

  // Shows the bubble.
  static void ShowBubble(content::WebContents* web_contents,
                         ManagePasswordsIconView* icon_view,
                         BubbleDisplayReason reason);

  // Closes any existing bubble.
  static void CloseBubble(BubbleDismissalReason reason);

  // Whether the bubble is currently showing.
  static bool IsShowing();

 private:
  enum ColumnSetType {
    // | | (FILL, FILL) | |
    // Used for the bubble's header, the credentials list, and for simple
    // messages like "No passwords".
    SINGLE_VIEW_COLUMN_SET = 0,

    // | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
    // Used for buttons at the bottom of the bubble which should nest at the
    // bottom-right corner.
    DOUBLE_BUTTON_COLUMN_SET = 1,

    // | | (LEADING, CENTER) | | (TRAILING, CENTER) | |
    // Used for buttons at the bottom of the bubble which should occupy
    // the corners.
    LINK_BUTTON_COLUMN_SET = 2,
  };

  ManagePasswordsBubbleView(content::WebContents* web_contents,
                            views::View* anchor_view,
                            ManagePasswordsIconView* icon_view,
                            BubbleDisplayReason reason);
  virtual ~ManagePasswordsBubbleView();

  // Construct an appropriate ColumnSet for the given |type|, and add it
  // to |layout|.
  void BuildColumnSet(views::GridLayout* layout, ColumnSetType type);

  // If the bubble is not anchored to a view, places the bubble in the top
  // right (left in RTL) of the |screen_bounds| that contain |web_contents_|'s
  // browser window. Because the positioning is based on the size of the
  // bubble, this must be called after the bubble is created.
  void AdjustForFullscreen(const gfx::Rect& screen_bounds);

  void Close(BubbleDismissalReason reason);

  // views::BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Handles the event when the user changes an index of a combobox.
  virtual void OnPerformAction(views::Combobox* source) OVERRIDE;

  // Singleton instance of the Password bubble. The Password bubble can only be
  // shown on the active browser window, so there is no case in which it will be
  // shown twice at the same time.
  static ManagePasswordsBubbleView* manage_passwords_bubble_;

  ManagePasswordsBubbleModel* manage_passwords_bubble_model_;
  ManagePasswordsIconView* icon_view_;

  // The buttons that are shown in the bubble.
  views::BlueButton* save_button_;
  views::Combobox* refuse_combobox_;

  views::Link* manage_link_;
  views::LabelButton* done_button_;

  // We track the dismissal reason so we can log it correctly in the destructor.
  BubbleDismissalReason dismissal_reason_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
