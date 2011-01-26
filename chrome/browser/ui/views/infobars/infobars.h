// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBARS_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBARS_H_
#pragma once

#include "base/task.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/focus/focus_manager.h"

class InfoBarContainer;
class InfoBarTextButton;

namespace ui {
class SlideAnimation;
}

namespace views {
class ExternalFocusTracker;
class ImageButton;
class ImageView;
class Label;
}

// This file contains implementations for some general purpose InfoBars. See
// chrome/browser/tab_contents/infobar_delegate.h for the delegate interface(s)
// that you must implement to use these.

class InfoBarBackground : public views::Background {
 public:
  explicit InfoBarBackground(InfoBarDelegate::Type infobar_type);

  // Overridden from views::Background:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const;

 private:
  scoped_ptr<views::Background> gradient_background_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarBackground);
};

class InfoBar : public views::View,
                public views::ButtonListener,
                public views::FocusChangeListener,
                public ui::AnimationDelegate {
 public:
  explicit InfoBar(InfoBarDelegate* delegate);
  virtual ~InfoBar();

  InfoBarDelegate* delegate() const { return delegate_; }

  // Set a link to the parent InfoBarContainer. This must be set before the
  // InfoBar is added to the view hierarchy.
  void set_container(InfoBarContainer* container) { container_ = container; }

  // The target height of the InfoBar, regardless of what its current height
  // is (due to animation).
  static const double kDefaultTargetHeight;

  static const int kHorizontalPadding;
  static const int kIconLabelSpacing;
  static const int kButtonButtonSpacing;
  static const int kEndOfLabelSpacing;
  static const int kCloseButtonSpacing;
  static const int kButtonInLabelSpacing;

  // Overridden from views::View:
  virtual AccessibilityTypes::Role GetAccessibleRole();
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // Returns the available width of the View for use by child view layout,
  // excluding the close button.
  virtual int GetAvailableWidth() const;

  // Removes our associated InfoBarDelegate from the associated TabContents.
  // (Will lead to this InfoBar being closed).
  void RemoveInfoBar() const;

  void set_target_height(double height) { target_height_ = height; }

  ui::SlideAnimation* animation() { return animation_.get(); }

  // Returns a centered y-position of a control of height specified in
  // |prefsize| within the standard InfoBar height. Stable during an animation.
  int CenterY(const gfx::Size prefsize);

  // Returns a centered y-position of a control of height specified in
  // |prefsize| within the standard InfoBar height, adjusted according to the
  // current amount of animation offset the |parent| InfoBar currently has.
  // Changes during an animation.
  int OffsetY(views::View* parent, const gfx::Size prefsize);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(View* focused_before, View* focused_now);

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation);
  virtual void AnimationEnded(const ui::Animation* animation);

 private:
  friend class InfoBarContainer;

  // Starts animating the InfoBar open.
  void AnimateOpen();

  // Opens the InfoBar immediately.
  void Open();

  // Starts animating the InfoBar closed. It will not be closed until the
  // animation has completed, when |Close| will be called.
  void AnimateClose();

  // Closes the InfoBar immediately and removes it from its container. Notifies
  // the delegate that it has closed. The InfoBar is deleted after this function
  // is called.
  void Close();

  // Called when an InfoBar is added or removed from a view hierarchy to do
  // setup and shutdown.
  void InfoBarAdded();
  void InfoBarRemoved();

  // Destroys the external focus tracker, if present. If |restore_focus| is
  // true, restores focus to the view tracked by the focus tracker before doing
  // so.
  void DestroyFocusTracker(bool restore_focus);

  // Deletes this object (called after a return to the message loop to allow
  // the stack in ViewHierarchyChanged to unwind).
  void DeleteSelf();

  // The InfoBar's container
  InfoBarContainer* container_;

  // The InfoBar's delegate.
  InfoBarDelegate* delegate_;

  // The Close Button at the right edge of the InfoBar.
  views::ImageButton* close_button_;

  // The animation that runs when the InfoBar is opened or closed.
  scoped_ptr<ui::SlideAnimation> animation_;

  // Tracks and stores the last focused view which is not the InfoBar or any of
  // its children. Used to restore focus once the InfoBar is closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  // Used to delete this object after a return to the message loop.
  ScopedRunnableMethodFactory<InfoBar> delete_factory_;

  // The target height for the InfoBar.
  double target_height_;

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

class AlertInfoBar : public InfoBar {
 public:
  explicit AlertInfoBar(ConfirmInfoBarDelegate* delegate);
  virtual ~AlertInfoBar();

  // Overridden from views::View:
  virtual void Layout();

 protected:
  views::Label* label() const { return label_; }
  views::ImageView* icon() const { return icon_; }

 private:
  views::Label* label_;
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(AlertInfoBar);
};

class LinkInfoBar : public InfoBar,
                    public views::LinkController {
 public:
  explicit LinkInfoBar(LinkInfoBarDelegate* delegate);
  virtual ~LinkInfoBar();

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from views::View:
  virtual void Layout();

 private:
  LinkInfoBarDelegate* GetDelegate();

  views::ImageView* icon_;
  views::Label* label_1_;
  views::Label* label_2_;
  views::Link* link_;

  DISALLOW_COPY_AND_ASSIGN(LinkInfoBar);
};

class ConfirmInfoBar : public AlertInfoBar,
                       public views::LinkController  {
 public:
  explicit ConfirmInfoBar(ConfirmInfoBarDelegate* delegate);
  virtual ~ConfirmInfoBar();

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from views::View:
  virtual void Layout();

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from InfoBar:
  virtual int GetAvailableWidth() const;

 private:
  void Init();

  ConfirmInfoBarDelegate* GetDelegate();

  InfoBarTextButton* ok_button_;
  InfoBarTextButton* cancel_button_;
  views::Link* link_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};


#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBARS_H_
