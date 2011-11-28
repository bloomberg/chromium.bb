// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/download/download_shelf.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/views/controls/button/button.h"
#include "views/accessible_pane_view.h"
#include "views/controls/link_listener.h"
#include "views/mouse_watcher.h"

class BaseDownloadItemModel;
class Browser;
class BrowserView;
class DownloadItemView;

namespace ui {
class SlideAnimation;
}

namespace views {
class ImageButton;
class ImageView;
}

// DownloadShelfView is a view that contains individual views for each download,
// as well as a close button and a link to show all downloads.
//
// DownloadShelfView does not hold an infinite number of download views, rather
// it'll automatically remove views once a certain point is reached.
class DownloadShelfView : public views::AccessiblePaneView,
                          public ui::AnimationDelegate,
                          public DownloadShelf,
                          public views::ButtonListener,
                          public views::LinkListener,
                          public views::MouseWatcherListener {
 public:
  DownloadShelfView(Browser* browser, BrowserView* parent);
  virtual ~DownloadShelfView();

  // Sent from the DownloadItemView when the user opens an item.
  void OpenedDownload(DownloadItemView* view);

  // Implementation of View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Implementation of ui::AnimationDelegate.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // Implementation of views::LinkListener.
  // Invoked when the user clicks the 'show all downloads' link button.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Implementation of ButtonListener.
  // Invoked when the user clicks the close button. Asks the browser to
  // hide the download shelf.
  virtual void ButtonPressed(views::Button* button,
                             const views::Event& event) OVERRIDE;

  // Implementation of DownloadShelf.
  virtual void AddDownload(BaseDownloadItemModel* download_model) OVERRIDE;
  virtual bool IsShowing() const OVERRIDE;
  virtual bool IsClosing() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual Browser* browser() const OVERRIDE;

  // Implementation of MouseWatcherDelegate OVERRIDE.
  virtual void MouseMovedOutOfView();

  // Override views::FocusChangeListener method from AccessiblePaneView.
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

  // Removes a specified download view. The supplied view is deleted after
  // it's removed.
  void RemoveDownloadView(views::View* view);

 protected:
  // From AccessiblePaneView
  virtual views::View* GetDefaultFocusableChild() OVERRIDE;

 private:
  // Adds a View representing a download to this DownloadShelfView.
  // DownloadShelfView takes ownership of the View, and will delete it as
  // necessary.
  void AddDownloadView(DownloadItemView* view);

  // Paints the border.
  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE;

  // Returns true if the shelf is wide enough to show the first download item.
  bool CanFitFirstDownloadItem();

  // Called on theme change.
  void UpdateButtonColors();

  // Overridden from views::View.
  virtual void OnThemeChanged();

  // Called when the "close shelf" animation ended.
  void Closed();

  // Returns true if we can auto close. We can auto-close if all the items on
  // the shelf have been opened.
  bool CanAutoClose();

  // Called when any view |view| gains or loses focus. If it's one of our
  // DownloadItemView children, call SchedulePaint on its bounds
  // so that its focus rect is repainted.
  void SchedulePaintForDownloadItem(views::View* view);

  // Get the rect that perfectly surrounds a DownloadItemView so we can
  // draw a focus rect around it.
  gfx::Rect GetFocusRectBounds(const DownloadItemView* download_item_view);

  // The browser for this shelf.
  Browser* browser_;

  // The animation for adding new items to the shelf.
  scoped_ptr<ui::SlideAnimation> new_item_animation_;

  // The show/hide animation for the shelf itself.
  scoped_ptr<ui::SlideAnimation> shelf_animation_;

  // The download views. These are also child Views, and deleted when
  // the DownloadShelfView is deleted.
  std::vector<DownloadItemView*> download_views_;

  // An image displayed on the right of the "Show all downloads..." link.
  views::ImageView* arrow_image_;

  // Link for showing all downloads. This is contained as a child, and deleted
  // by View.
  views::Link* show_all_view_;

  // Button for closing the downloads. This is contained as a child, and
  // deleted by View.
  views::ImageButton* close_button_;

  // The window this shelf belongs to.
  BrowserView* parent_;

  // Whether we are auto-closing.
  bool auto_closed_;

  views::MouseWatcher mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
