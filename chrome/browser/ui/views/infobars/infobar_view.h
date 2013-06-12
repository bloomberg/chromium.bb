// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_container.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/focus/external_focus_tracker.h"

namespace ui {
class MenuModel;
}
namespace views {
class ImageButton;
class ImageView;
class Label;
class LabelButton;
class Link;
class LinkListener;
class MenuButton;
class MenuButtonListener;
class MenuRunner;
}

class InfoBarView : public InfoBar,
                    public views::View,
                    public views::ButtonListener,
                    public views::ExternalFocusTracker {
 public:
  InfoBarView(InfoBarService* owner, InfoBarDelegate* delegate);

  const SkPath& fill_path() const { return fill_path_; }
  const SkPath& stroke_path() const { return stroke_path_; }

 protected:
  static const int kButtonButtonSpacing;
  static const int kEndOfLabelSpacing;

  virtual ~InfoBarView();

  // Creates a label with the appropriate font and color for an infobar.
  views::Label* CreateLabel(const string16& text) const;

  // Creates a link with the appropriate font and color for an infobar.
  // NOTE: Subclasses must ignore link clicks if we're unowned.
  views::Link* CreateLink(const string16& text,
                          views::LinkListener* listener) const;

  // Creates a menu button with an infobar-specific appearance.
  // NOTE: Subclasses must ignore button presses if we're unowned.
  static views::MenuButton* CreateMenuButton(
      const string16& text,
      views::MenuButtonListener* menu_button_listener);

  // Creates a button with an infobar-specific appearance.
  // NOTE: Subclasses must ignore button presses if we're unowned.
  static views::LabelButton* CreateLabelButton(views::ButtonListener* listener,
                                               const string16& text,
                                               bool needs_elevation);

  // views::View:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

  // views::ButtonListener:
  // NOTE: This must not be called if we're unowned.  (Subclasses should ignore
  // calls to ButtonPressed() in this case.)
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

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

  // Shows a menu at the specified position.
  // NOTE: This must not be called if we're unowned.  (Subclasses should ignore
  // calls to RunMenu() in this case.)
  void RunMenuAt(ui::MenuModel* menu_model,
                 views::MenuButton* button,
                 views::MenuItemView::AnchorPosition anchor);

 private:
  static const int kHorizontalPadding;

  // InfoBar:
  virtual void PlatformSpecificShow(bool animate) OVERRIDE;
  virtual void PlatformSpecificHide(bool animate) OVERRIDE;
  virtual void PlatformSpecificOnHeightsRecalculated() OVERRIDE;

  // views::View:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

  // views::ExternalFocusTracker:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;

  // The optional icon at the left edge of the InfoBar.
  views::ImageView* icon_;

  // The close button at the right edge of the InfoBar.
  views::ImageButton* close_button_;

  // The paths for the InfoBarBackground to draw, sized according to the heights
  // above.
  SkPath fill_path_;
  SkPath stroke_path_;

  // Used to run the menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
