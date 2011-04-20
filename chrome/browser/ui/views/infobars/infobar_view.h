// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
#pragma once

#include "base/task.h"
#include "chrome/browser/ui/views/infobars/infobar.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"
#include "views/controls/button/button.h"
#include "views/focus/focus_manager.h"

class SkPath;

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

class InfoBarView : public InfoBar,
                    public views::View,
                    public views::ButtonListener,
                    public views::FocusChangeListener {
 public:
  explicit InfoBarView(InfoBarDelegate* delegate);

  SkPath* fill_path() const { return fill_path_.get(); }
  SkPath* stroke_path() const { return stroke_path_.get(); }

 protected:
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
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Returns the minimum width the content (that is, everything between the icon
  // and the close button) can be shrunk to.  This is used to prevent the close
  // button from overlapping views that cannot be shrunk any further.
  virtual int ContentMinimumWidth() const;

  // These return x coordinates delimiting the usable area for subclasses to lay
  // out their controls.
  int StartX() const;
  int EndX() const;

  // Convenience getter.
  const InfoBarContainer::Delegate* container_delegate() const;

 private:
  static const int kHorizontalPadding;

  // InfoBar:
  virtual void PlatformSpecificHide(bool animate) OVERRIDE;
  virtual void PlatformSpecificOnHeightsRecalculated() OVERRIDE;

  // views::View:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

  // views::FocusChangeListener:
  virtual void FocusWillChange(View* focused_before,
                               View* focused_now) OVERRIDE;

  // Destroys the external focus tracker, if present. If |restore_focus| is
  // true, restores focus to the view tracked by the focus tracker before doing
  // so.
  void DestroyFocusTracker(bool restore_focus);

  // Deletes this object (called after a return to the message loop to allow
  // the stack in ViewHierarchyChanged to unwind).
  void DeleteSelf();

  // The optional icon at the left edge of the InfoBar.
  views::ImageView* icon_;

  // The close button at the right edge of the InfoBar.
  views::ImageButton* close_button_;

  // Tracks and stores the last focused view which is not the InfoBar or any of
  // its children. Used to restore focus once the InfoBar is closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  // Used to delete this object after a return to the message loop.
  ScopedRunnableMethodFactory<InfoBarView> delete_factory_;

  // The paths for the InfoBarBackground to draw, sized according to the heights
  // above.
  scoped_ptr<SkPath> fill_path_;
  scoped_ptr<SkPath> stroke_path_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
