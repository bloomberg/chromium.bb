// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "ui/views/controls/image_view.h"

class LocationBarView;
namespace views {
class MenuRunner;
}

// PageActionImageView is used by the LocationBarView to display the icon for a
// given PageAction and notify the extension when the icon is clicked.
class PageActionImageView : public views::ImageView,
                            public ImageLoadingTracker::Observer,
                            public ExtensionContextMenuModel::PopupDelegate,
                            public views::Widget::Observer,
                            public content::NotificationObserver {
 public:
  PageActionImageView(LocationBarView* owner,
                      ExtensionAction* page_action);
  virtual ~PageActionImageView();

  ExtensionAction* page_action() { return page_action_; }

  int current_tab_id() { return current_tab_id_; }

  void set_preview_enabled(bool preview_enabled) {
    preview_enabled_ = preview_enabled;
  }

  // Overridden from view.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual void ShowContextMenu(const gfx::Point& p,
                               bool is_mouse_gesture) OVERRIDE;

  // Overridden from ImageLoadingTracker.
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index) OVERRIDE;

  // Overridden from ExtensionContextMenuModelModel::Delegate
  virtual void InspectPopup(ExtensionAction* action) OVERRIDE;

  // Overridden from views::Widget::Observer
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called to notify the PageAction that it should determine whether to be
  // visible or hidden. |contents| is the TabContents that is active, |url| is
  // the current page URL.
  void UpdateVisibility(TabContents* contents, const GURL& url);

  // Either notify listeners or show a popup depending on the page action.
  void ExecuteAction(int button, bool inspect_with_devtools);

 private:
  // Hides the active popup, if there is one.
  void HidePopup();

  // The location bar view that owns us.
  LocationBarView* owner_;

  // The PageAction that this view represents. The PageAction is not owned by
  // us, it resides in the extension of this particular profile.
  ExtensionAction* page_action_;

  // A cache of bitmaps the page actions might need to show, mapped by path.
  typedef std::map<std::string, SkBitmap> PageActionMap;
  PageActionMap page_action_icons_;

  // The object that is waiting for the image loading to complete
  // asynchronously.
  ImageLoadingTracker tracker_;

  // The tab id we are currently showing the icon for.
  int current_tab_id_;

  // The URL we are currently showing the icon for.
  GURL current_url_;

  // The string to show for a tooltip;
  std::string tooltip_;

  // This is used for post-install visual feedback. The page_action icon is
  // briefly shown even if it hasn't been enabled by its extension.
  bool preview_enabled_;

  // The current popup and the button it came from.  NULL if no popup.
  ExtensionPopup* popup_;

  content::NotificationRegistrar registrar_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PageActionImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
