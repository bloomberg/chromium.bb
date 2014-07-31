// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
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
                            public ExtensionContextMenuModel::PopupDelegate,
                            public views::WidgetObserver,
                            public views::ContextMenuController,
                            public ExtensionActionIconFactory::Observer {
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
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // Overridden from ExtensionContextMenuModel::Delegate
  virtual void InspectPopup() OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Overridden from views::ContextMenuController.
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // Overriden from ExtensionActionIconFactory::Observer.
  virtual void OnIconUpdated() OVERRIDE;

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
  // Overridden from View.
  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE;

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

  // The object that will be used to get the page action icon for us.
  // It may load the icon asynchronously (in which case the initial icon
  // returned by the factory will be transparent), so we have to observe it for
  // updates to the icon.
  scoped_ptr<ExtensionActionIconFactory> icon_factory_;

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

  // The extension command accelerator this page action is listening for (to
  // show the popup).
  scoped_ptr<ui::Accelerator> page_action_keybinding_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PageActionImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
