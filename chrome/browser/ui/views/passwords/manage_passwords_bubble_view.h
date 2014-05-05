// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "chrome/browser/ui/views/passwords/save_password_refusal_combobox_model.h"
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

class ManagePasswordsBubbleView : public ManagePasswordsBubble,
                                  public views::BubbleDelegateView,
                                  public views::ButtonListener,
                                  public views::ComboboxListener,
                                  public views::LinkListener {
 public:
  // Shows the bubble.
  static void ShowBubble(content::WebContents* web_contents,
                         DisplayReason reason);

  // Closes any existing bubble.
  static void CloseBubble();

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
                            DisplayReason reason);
  virtual ~ManagePasswordsBubbleView();

  // Construct an appropriate ColumnSet for the given |type|, and add it
  // to |layout|.
  void BuildColumnSet(views::GridLayout* layout, ColumnSetType type);

  // If the bubble is not anchored to a view, places the bubble in the top
  // right (left in RTL) of the |screen_bounds| that contain |web_contents_|'s
  // browser window. Because the positioning is based on the size of the
  // bubble, this must be called after the bubble is created.
  void AdjustForFullscreen(const gfx::Rect& screen_bounds);

  // Close the bubble.
  void Close();

  // Close the bubble without triggering UMA logging. We need this for the
  // moment, as the current implementation can close the bubble without user
  // interaction; we want to ensure that doesn't show up as a user action in
  // our counts.
  //
  // TODO(mkwst): Throw this away once the icon is no longer responsible for
  // creating the bubble.
  void CloseWithoutLogging();

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

  // The views that are shown in the bubble.
  views::BlueButton* save_button_;
  views::Link* manage_link_;
  views::LabelButton* done_button_;

  // The combobox doesn't take ownership of it's model. If we created a combobox
  // we need to ensure that we delete the model here, and because the combobox
  // uses the model in it's destructor, we need to make sure we delete the model
  // _after_ the combobox itself is deleted.
  scoped_ptr<SavePasswordRefusalComboboxModel> combobox_model_;
  scoped_ptr<views::Combobox> refuse_combobox_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
