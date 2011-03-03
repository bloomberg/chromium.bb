// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
#pragma once

#include "base/task.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/controls/button/button.h"
#include "views/focus/focus_manager.h"

class InfoBarContainer;
class InfoBarDelegate;

namespace ui {
class SlideAnimation;
}

namespace views {
class ExternalFocusTracker;
class ImageButton;
class ImageView;
class Label;
class Link;
class LinkController;
class MenuButton;
class TextButton;
class ViewMenuDelegate;
}

// TODO(pkasting): infobar_delegate.h forward declares "class InfoBar" but the
// definitions are (right now) completely port-specific.  This stub class will
// be turned into the cross-platform base class for InfoBar views (in the MVC
// sense).  Right now it's just here so the various InfoBarDelegates can
// continue to return an InfoBar*, it doesn't do anything.
class InfoBar {
 public:
  explicit InfoBar(InfoBarDelegate* delegate) {}
  virtual ~InfoBar() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

class InfoBarView : public InfoBar,
                    public views::View,
                    public views::ButtonListener,
                    public views::FocusChangeListener,
                    public ui::AnimationDelegate {
 public:
  explicit InfoBarView(InfoBarDelegate* delegate);

  InfoBarDelegate* delegate() const { return delegate_; }

  // Set a link to the parent InfoBarContainer. This must be set before the
  // InfoBar is added to the view hierarchy.
  void set_container(InfoBarContainer* container) { container_ = container; }

  // Makes the infobar visible.  If |animate| is true, the infobar is then
  // animated to full size.
  void Show(bool animate);

  // Makes the infobar hidden.  If |animate| is true, the infobar is first
  // animated to zero size.  Once the infobar is hidden, it is removed from its
  // container (triggering its deletion), and its delegate is closed.
  void Hide(bool animate);

  // Closes the InfoBar immediately and removes it from its container. Notifies
  // the delegate that it has closed. The InfoBar is deleted after this function
  // is called.
  void Close();

  // Paint the arrow on |canvas|. |arrow_center_x| indicates the
  // desired location of the center of the arrow in the |outer_view|
  // coordinate system.
  void PaintArrow(gfx::Canvas* canvas, View* outer_view, int arrow_center_x);

 protected:
  // The target height of the InfoBar, regardless of what its current height
  // is (due to animation).
  static const int kDefaultTargetHeight;
  static const int kButtonButtonSpacing;
  static const int kEndOfLabelSpacing;

  virtual ~InfoBarView();

  // Creates a label with the appropriate font and color for an infobar.
  static views::Label* CreateLabel(const string16& text);

  // Creates a link with the appropriate font and color for an infobar.
  static views::Link* CreateLink(const string16& text,
                                 views::LinkController* controller,
                                 const SkColor& background_color);

  // Creates a menu button with an infobar-specific appearance.
  static views::MenuButton* CreateMenuButton(
      const string16& text,
      bool normal_has_border,
      views::ViewMenuDelegate* menu_delegate);

  // Creates a text button with an infobar-specific appearance.
  static views::TextButton* CreateTextButton(views::ButtonListener* listener,
                                             const string16& text,
                                             bool needs_elevation);

  // views::View:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child)
      OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event)
      OVERRIDE;

  // ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Returns the minimum width the content (that is, everything between the icon
  // and the close button) can be shrunk to.  This is used to prevent the close
  // button from overlapping views that cannot be shrunk any further.
  virtual int ContentMinimumWidth() const;

  // Removes our associated InfoBarDelegate from the associated TabContents.
  // (Will lead to this InfoBar being closed).
  void RemoveInfoBar() const;

  void set_target_height(int height) { target_height_ = height; }

  ui::SlideAnimation* animation() { return animation_.get(); }

  // These return x coordinates delimiting the usable area for subclasses to lay
  // out their controls.
  int StartX() const;
  int EndX() const;

  // Returns a centered y-position of a control of height specified in
  // |prefsize| within the standard InfoBar height. Stable during an animation.
  int CenterY(const gfx::Size prefsize) const;

  // Returns a centered y-position of a control of height specified in
  // |prefsize| within the standard InfoBar height, adjusted according to the
  // current amount of animation offset the |parent| InfoBar currently has.
  // Changes during an animation.
  int OffsetY(const gfx::Size prefsize) const;

 private:
  static const int kHorizontalPadding;

  // views::View:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::FocusChangeListener:
  virtual void FocusWillChange(View* focused_before, View* focused_now)
      OVERRIDE;

  // ui::AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

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

  // The optional icon at the left edge of the InfoBar.
  views::ImageView* icon_;

  // The close button at the right edge of the InfoBar.
  views::ImageButton* close_button_;

  // The animation that runs when the InfoBar is opened or closed.
  scoped_ptr<ui::SlideAnimation> animation_;

  // Tracks and stores the last focused view which is not the InfoBar or any of
  // its children. Used to restore focus once the InfoBar is closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  // Used to delete this object after a return to the message loop.
  ScopedRunnableMethodFactory<InfoBarView> delete_factory_;

  // The target height for the InfoBarView.
  int target_height_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
