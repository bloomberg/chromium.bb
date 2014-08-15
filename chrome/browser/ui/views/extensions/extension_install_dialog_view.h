// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_

#include "chrome/browser/extensions/extension_install_prompt.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

typedef std::vector<base::string16> PermissionDetails;
class ExpandableContainerView;

namespace content {
class PageNavigator;
}

namespace extensions {
class ExperienceSamplingEvent;
}

namespace views {
class GridLayout;
class ImageButton;
class Label;
class Link;
}

// A custom scrollable view implementation for the dialog.
class CustomScrollableView : public views::View {
 public:
  CustomScrollableView();
  virtual ~CustomScrollableView();

 private:
  virtual void Layout() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CustomScrollableView);
};

// Implements the extension installation dialog for TOOLKIT_VIEWS.
class ExtensionInstallDialogView : public views::DialogDelegateView,
                                   public views::LinkListener,
                                   public views::ButtonListener {
 public:
  ExtensionInstallDialogView(
      content::PageNavigator* navigator,
      ExtensionInstallPrompt::Delegate* delegate,
      scoped_refptr<ExtensionInstallPrompt::Prompt> prompt);
  virtual ~ExtensionInstallDialogView();

  // Returns the interior ScrollView of the dialog. This allows us to inspect
  // the contents of the DialogView.
  const views::ScrollView* scroll_view() const { return scroll_view_; }

  // Called when one of the child elements has expanded/collapsed.
  void ContentsChanged();

 private:
  // views::DialogDelegateView:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Experimental: Toggles inline permission explanations with an animation.
  void ToggleInlineExplanations();

  // Creates a layout consisting of dialog header, extension name and icon.
  views::GridLayout* CreateLayout(
      views::View* parent,
      int left_column_width,
      int column_set_id,
      bool single_detail_row) const;

  bool is_inline_install() const {
    return prompt_->type() == ExtensionInstallPrompt::INLINE_INSTALL_PROMPT;
  }

  bool is_bundle_install() const {
    return prompt_->type() == ExtensionInstallPrompt::BUNDLE_INSTALL_PROMPT;
  }

  bool is_external_install() const {
    return prompt_->type() == ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT;
  }

  // Updates the histogram that holds installation accepted/aborted data.
  void UpdateInstallResultHistogram(bool accepted) const;

  // Updates the histogram that holds data about whether "Show details" or
  // "Show permissions" links were shown and/or clicked.
  void UpdateLinkActionHistogram(int action_type) const;

  content::PageNavigator* navigator_;
  ExtensionInstallPrompt::Delegate* delegate_;
  scoped_refptr<ExtensionInstallPrompt::Prompt> prompt_;

  // The scroll view containing all the details for the dialog (including all
  // collapsible/expandable sections).
  views::ScrollView* scroll_view_;

  // The container view for the scroll view.
  CustomScrollableView* scrollable_;

  // The container for the simpler view with only the dialog header and the
  // extension icon. Used for the experiment where the permissions are
  // initially hidden when the dialog shows.
  CustomScrollableView* scrollable_header_only_;

  // The preferred size of the dialog.
  gfx::Size dialog_size_;

  // Experimental: "Show details" link to expand inline explanations and reveal
  // permision dialog.
  views::Link* show_details_link_;

  // Experimental: Label for showing information about the checkboxes.
  views::Label* checkbox_info_label_;

  // Experimental: Contains pointers to inline explanation views.
  typedef std::vector<ExpandableContainerView*> InlineExplanations;
  InlineExplanations inline_explanations_;

  // Experimental: Number of unchecked checkboxes in the permission list.
  // If this becomes zero, the accept button is enabled, otherwise disabled.
  int unchecked_boxes_;

  // ExperienceSampling: Track this UI event.
  scoped_ptr<extensions::ExperienceSamplingEvent> sampling_event_;

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

// A simple view that prepends a view with a checkbox with the help of a grid
// layout. Used for the permission experiment.
// TODO(meacer): Remove once the experiment is completed.
class CheckboxedView : public views::View {
 public:
  CheckboxedView(views::View* view, views::ButtonListener* listener);
 private:
  DISALLOW_COPY_AND_ASSIGN(CheckboxedView);
};

// A view to display text with an expandable details section.
class ExpandableContainerView : public views::View,
                                public views::ButtonListener,
                                public views::LinkListener,
                                public gfx::AnimationDelegate {
 public:
  ExpandableContainerView(ExtensionInstallDialogView* owner,
                          const base::string16& description,
                          const PermissionDetails& details,
                          int horizontal_space,
                          bool parent_bulleted,
                          bool show_expand_link,
                          bool lighter_color_details);
  virtual ~ExpandableContainerView();

  // views::View:
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // gfx::AnimationDelegate:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;

  // Expand/Collapse the detail section for this ExpandableContainerView.
  void ToggleDetailLevel();

  // Expand the detail section without any animation.
  // TODO(meacer): Remove once the experiment is completed.
  void ExpandWithoutAnimation();

 private:
  // A view which displays all the details of an IssueAdviceInfoEntry.
  class DetailsView : public views::View {
   public:
    explicit DetailsView(int horizontal_space, bool parent_bulleted,
                         bool lighter_color);
    virtual ~DetailsView() {}

    // views::View:
    virtual gfx::Size GetPreferredSize() const OVERRIDE;

    void AddDetail(const base::string16& detail);

    // Animates this to be a height proportional to |state|.
    void AnimateToState(double state);

   private:
    views::GridLayout* layout_;
    double state_;

    // Whether the detail text should be shown with a lighter color.
    bool lighter_color_;

    DISALLOW_COPY_AND_ASSIGN(DetailsView);
  };

  // The dialog that owns |this|. It's also an ancestor in the View hierarchy.
  ExtensionInstallDialogView* owner_;

  // A view for showing |issue_advice.details|.
  DetailsView* details_view_;

  // The 'more details' link shown under the heading (changes to 'hide details'
  // when the details section is expanded).
  views::Link* more_details_;

  gfx::SlideAnimation slide_animation_;

  // The up/down arrow next to the 'more detail' link (points up/down depending
  // on whether the details section is expanded).
  views::ImageButton* arrow_toggle_;

  // Whether the details section is expanded.
  bool expanded_;

  DISALLOW_COPY_AND_ASSIGN(ExpandableContainerView);
};

void ShowExtensionInstallDialogImpl(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> prompt);

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
