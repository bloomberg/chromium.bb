// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/common/extensions/extension_action.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class LocationBarView;

namespace content {
class WebContents;
}
namespace views {
class MenuRunner;
}

// PageActionImageView is used by the LocationBarView to display the icon for a
// given PageAction and notify the extension when the icon is clicked.
class PageActionImageView : public views::ImageView,
                            public ImageLoadingTracker::Observer,
                            public ExtensionContextMenuModel::PopupDelegate,
                            public views::WidgetObserver,
                            public views::ContextMenuController,
                            public content::NotificationObserver,
                            public ExtensionAction::IconAnimation::Observer {
 public:
  PageActionImageView(LocationBarView* owner,
                      ExtensionAction* page_action,
                      Browser* browser);
  virtual ~PageActionImageView();

  ExtensionAction* page_action() { return page_action_; }

  int current_tab_id() { return current_tab_id_; }

  void set_preview_enabled(bool preview_enabled) {
    preview_enabled_ = preview_enabled;
  }

  // Overridden from views::View:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // Overridden from ImageLoadingTracker:
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

  // Overridden from ExtensionContextMenuModel::Delegate
  virtual void InspectPopup(ExtensionAction* action) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // Overridden from views::ContextMenuController.
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& point) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  // Called to notify the PageAction that it should determine whether to be
  // visible or hidden. |contents| is the WebContents that is active, |url| is
  // the current page URL.
  void UpdateVisibility(content::WebContents* contents, const GURL& url);

  // Either notify listeners or show a popup depending on the page action.
  void ExecuteAction(ExtensionPopup::ShowAction show_action);

 private:
  // Overridden from ExtensionAction::IconAnimation::Observer:
  virtual void OnIconChanged() OVERRIDE;

  // Shows the popup, with the given URL.
  void ShowPopupWithURL(const GURL& popup_url,
                        ExtensionPopup::ShowAction show_action);

  // Hides the active popup, if there is one.
  void HidePopup();

  // The location bar view that owns us.
  LocationBarView* owner_;

  // The PageAction that this view represents. The PageAction is not owned by
  // us, it resides in the extension of this particular profile.
  ExtensionAction* page_action_;

  // The corresponding browser.
  Browser* browser_;

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

  // The extension command accelerator this page action is listening for (to
  // show the popup).
  scoped_ptr<ui::Accelerator> page_action_keybinding_;

  // The extension command accelerator this script badge is listening for (to
  // show the popup).
  scoped_ptr<ui::Accelerator> script_badge_keybinding_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  // Fade-in animation for the icon with observer scoped to this.
  ExtensionAction::IconAnimation::ScopedObserver
      scoped_icon_animation_observer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PageActionImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
