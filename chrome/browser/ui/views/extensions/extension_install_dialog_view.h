// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

typedef std::vector<base::string16> PermissionDetails;
class Profile;

namespace content {
class PageNavigator;
}

namespace extensions {
class ExperienceSamplingEvent;
}

namespace ui {
class ResourceBundle;
}

namespace views {
class GridLayout;
class ImageButton;
class Link;
}

// Implements the extension installation dialog for TOOLKIT_VIEWS.
class ExtensionInstallDialogView : public views::DialogDelegateView,
                                   public views::LinkListener {
 public:
  ExtensionInstallDialogView(
      Profile* profile,
      content::PageNavigator* navigator,
      const ExtensionInstallPrompt::DoneCallback& done_callback,
      std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt);
  ~ExtensionInstallDialogView() override;

  // Returns the interior ScrollView of the dialog. This allows us to inspect
  // the contents of the DialogView.
  const views::ScrollView* scroll_view() const { return scroll_view_; }

  static void SetInstallButtonDelayForTesting(int timeout_in_ms);

 private:
  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  int GetDefaultDialogButton() const override;
  bool Cancel() override;
  bool Accept() override;
  ui::ModalType GetModalType() const override;
  void Layout() override;
  views::View* CreateExtraView() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // Initializes the dialog view, adding in permissions if they exist.
  void InitView();

  // Enables the install button and updates the dialog buttons.
  void EnableInstallButton();

  // Adds permissions of |perm_type| to the dialog view if they exist.
  bool AddPermissions(views::GridLayout* layout,
                      ui::ResourceBundle& rb,
                      int column_set_id,
                      int left_column_width,
                      ExtensionInstallPrompt::PermissionsType perm_type);

  // Creates a layout consisting of dialog header, extension name and icon.
  views::GridLayout* CreateLayout(int left_column_width, int column_set_id);

  bool is_external_install() const {
    return prompt_->type() == ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT;
  }

  // Updates the histogram that holds installation accepted/aborted data.
  void UpdateInstallResultHistogram(bool accepted) const;

  Profile* profile_;
  content::PageNavigator* navigator_;
  ExtensionInstallPrompt::DoneCallback done_callback_;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt_;

  // The container view that contains all children (heading, icon, webstore
  // data, and the scroll view with permissions etc.), excluding the buttons,
  // which are added automatically by the dialog system.
  View* container_;

  // The scroll view containing all the details for the dialog (including all
  // collapsible/expandable sections).
  views::ScrollView* scroll_view_;

  // ExperienceSampling: Track this UI event.
  std::unique_ptr<extensions::ExperienceSamplingEvent> sampling_event_;

  // Set to true once the user's selection has been received and the callback
  // has been run.
  bool handled_result_;

  // Used to delay the activation of the install button.
  base::OneShotTimer timer_;

  // Used to determine whether the install button should be enabled.
  bool install_button_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogView);
};

// A simple view that prepends a view with a bullet with the help of a grid
// layout.
class BulletedView : public views::View {
 public:
  explicit BulletedView(views::View* view);

 private:
  DISALLOW_COPY_AND_ASSIGN(BulletedView);
};

// A view to display text with an expandable details section.
class ExpandableContainerView : public views::View,
                                public views::ButtonListener,
                                public views::LinkListener,
                                public gfx::AnimationDelegate {
 public:
  ExpandableContainerView(const PermissionDetails& details,
                          int horizontal_space,
                          bool parent_bulleted);
  ~ExpandableContainerView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  // A view which displays all the details of an IssueAdviceInfoEntry.
  class DetailsView : public views::View {
   public:
    DetailsView(int horizontal_space, bool parent_bulleted);
    ~DetailsView() override {}

    // views::View:
    gfx::Size CalculatePreferredSize() const override;

    void AddDetail(const base::string16& detail);

    // Animates this to be a height proportional to |state|.
    void AnimateToState(double state);

   private:
    views::GridLayout* layout_;
    double state_;

    DISALLOW_COPY_AND_ASSIGN(DetailsView);
  };

  // Expand/Collapse the detail section for this ExpandableContainerView.
  void ToggleDetailLevel();

  // Updates |arrow_toggle_| according to the given state.
  void UpdateArrowToggle(bool expanded);

  // A view for showing |issue_advice.details|.
  DetailsView* details_view_;

  gfx::SlideAnimation slide_animation_;

  // The 'more details' link shown under the heading (changes to 'hide details'
  // when the details section is expanded).
  views::Link* more_details_;

  // The up/down arrow next to the 'more detail' link (points up/down depending
  // on whether the details section is expanded).
  views::ImageButton* arrow_toggle_;

  // Whether the details section is expanded.
  bool expanded_;

  DISALLOW_COPY_AND_ASSIGN(ExpandableContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
