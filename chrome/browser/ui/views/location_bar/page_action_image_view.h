// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
#pragma once

#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "views/controls/image_view.h"

class LocationBarView;
namespace views {
class Menu2;
};

// PageActionImageView is used by the LocationBarView to display the icon for a
// given PageAction and notify the extension when the icon is clicked.
class PageActionImageView : public views::ImageView,
                            public ImageLoadingTracker::Observer,
                            public ExtensionContextMenuModel::PopupDelegate,
                            public ExtensionPopup::Observer {
 public:
  PageActionImageView(LocationBarView* owner,
                      Profile* profile,
                      ExtensionAction* page_action);
  virtual ~PageActionImageView();

  ExtensionAction* page_action() { return page_action_; }

  int current_tab_id() { return current_tab_id_; }

  void set_preview_enabled(bool preview_enabled) {
    preview_enabled_ = preview_enabled;
  }

  // Overridden from view.
  virtual AccessibilityTypes::Role GetAccessibleRole();
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual bool OnKeyPressed(const views::KeyEvent& e);
  virtual void ShowContextMenu(const gfx::Point& p, bool is_mouse_gesture);

  // Overridden from ImageLoadingTracker.
  virtual void OnImageLoaded(
      SkBitmap* image, ExtensionResource resource, int index);

  // Overridden from ExtensionContextMenuModelModel::Delegate
  virtual void InspectPopup(ExtensionAction* action);

  // Overridden from ExtensionPopup::Observer
  virtual void ExtensionPopupIsClosing(ExtensionPopup* popup);

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

  // The current profile (not owned by us).
  Profile* profile_;

  // The PageAction that this view represents. The PageAction is not owned by
  // us, it resides in the extension of this particular profile.
  ExtensionAction* page_action_;

  // A cache of bitmaps the page actions might need to show, mapped by path.
  typedef std::map<std::string, SkBitmap> PageActionMap;
  PageActionMap page_action_icons_;

  // The context menu for this page action.
  scoped_refptr<ExtensionContextMenuModel> context_menu_contents_;
  scoped_ptr<views::Menu2> context_menu_menu_;

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

  DISALLOW_IMPLICIT_CONSTRUCTORS(PageActionImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
