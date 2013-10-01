// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILE_RESET_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILE_RESET_BUBBLE_VIEW_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profile_resetter/profile_reset_callback.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

namespace content {
class PageNavigator;
}

namespace views {
class Checkbox;
class ImageButton;
class LabelButton;
class Link;
}

class Profile;

// ProfileResetBubbleView warns the user that a settings reset might be needed.
// It is intended to be used as the content of a bubble anchored off of the
// Chrome toolbar.
class ProfileResetBubbleView : public views::BubbleDelegateView,
                                public views::ButtonListener,
                                public views::LinkListener {
 public:
  static void ShowBubble(views::View* anchor_view,
                         content::PageNavigator* navigator,
                         Profile* profile,
                         const ProfileResetCallback& reset_callback);

  // views::BubbleDelegateView methods.
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual void Init() OVERRIDE;

  // views::WidgetDelegate method.
  virtual void WindowClosing() OVERRIDE;

 private:
  ProfileResetBubbleView(views::View* anchor_view,
                         content::PageNavigator* navigator,
                         Profile* profile,
                         const ProfileResetCallback& reset_callback);

  virtual ~ProfileResetBubbleView();

  static bool IsShowing() { return reset_bubble_ != NULL; }

  // Reset all child views members and remove children from view hierarchy.
  void ResetAllChildren();

  // Sets up the layout manager and set the report checkbox to the value passed
  // in |report_checked|.
  void SetupLayoutManager(bool report_checked);

  // views::ButtonListener method.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener method.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Callback from the profile resetter, once it's done.
  void OnResetProfileSettingsDone();

  // The reset bubble, if we're showing one.
  static ProfileResetBubbleView* reset_bubble_;

  // The numer of times the user ignored the bubble before finally choosing to
  // reset.
  static int num_ignored_bubbles_;

  struct Controls {
    Controls() {
      Reset();
    }
    void Reset() {
      reset_button = NULL;
      no_thanks_button = NULL;
      help_button = NULL;
      report_settings_checkbox = NULL;
    }

    // Button for the user to confirm a settings reset.
    views::LabelButton* reset_button;

    // Button for the user to refuse a settings reset.
    views::LabelButton* no_thanks_button;

    // Button for the user to get more info about reporting settings.
    views::ImageButton* help_button;

    // Checkbox for the user to choose to report the settings or not.
    views::Checkbox* report_settings_checkbox;
  } controls_;

  // A version of the help image that is brighter.
  gfx::ImageSkia brighter_help_image_;

  // Used for opening the learn more link.
  content::PageNavigator* navigator_;

  // Used to access profile specific stuff like the global error or readable
  // feedback.
  Profile* profile_;

  // Callback to the code that takes care of the profile reset.
  ProfileResetCallback reset_callback_;

  // Remembers if we are currently resetting or not.
  bool resetting_;

  // Remembers if the reset button was hit before closing the bubble.
  bool chose_to_reset_;

  // Toggles when the user clicks on the |help_button_| to identify if we should
  // show the help pane or not.
  bool show_help_pane_;

  // To cancel pending callbacks after destruction.
  base::WeakPtrFactory<ProfileResetBubbleView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILE_RESET_BUBBLE_VIEW_H_
