// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_action_view_delegate.h"
#include "ui/views/controls/image_view.h"

class Browser;
class ExtensionAction;
class LocationBarView;

namespace content {
class WebContents;
}

// PageActionImageView is used by the LocationBarView to display the icon for a
// given PageAction and notify the extension when the icon is clicked.
class PageActionImageView : public ExtensionActionViewDelegate,
                            public views::ImageView {
 public:
  PageActionImageView(LocationBarView* owner,
                      ExtensionAction* page_action,
                      Browser* browser);
  virtual ~PageActionImageView();

  void set_preview_enabled(bool preview_enabled) {
    preview_enabled_ = preview_enabled;
  }
  ExtensionAction* extension_action() {
    return view_controller_->extension_action();
  }
  ExtensionActionViewController* view_controller() {
    return view_controller_.get();
  }

  // Overridden from views::View:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Called to notify the PageAction that it should determine whether to be
  // visible or hidden. |contents| is the WebContents that is active.
  void UpdateVisibility(content::WebContents* contents);

 private:
  static const char kViewClassName[];

  // Overridden from View.
  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE;

  // Overridden from ExtensionActionViewDelegate:
  virtual void OnIconUpdated() OVERRIDE;
  virtual views::View* GetAsView() OVERRIDE;
  virtual bool IsShownInMenu() OVERRIDE;
  virtual views::FocusManager* GetFocusManagerForAccelerator() OVERRIDE;
  virtual views::Widget* GetParentForContextMenu() OVERRIDE;
  virtual ExtensionActionViewController* GetPreferredPopupViewController()
      OVERRIDE;
  virtual views::View* GetReferenceViewForPopup() OVERRIDE;
  virtual views::MenuButton* GetContextMenuButton() OVERRIDE;
  virtual content::WebContents* GetCurrentWebContents() OVERRIDE;
  virtual void HideActivePopup() OVERRIDE;

  // The controller for this ExtensionAction view.
  scoped_ptr<ExtensionActionViewController> view_controller_;

  // The location bar view that owns us.
  LocationBarView* owner_;

  // The string to show for a tooltip;
  std::string tooltip_;

  // This is used for post-install visual feedback. The page_action icon is
  // briefly shown even if it hasn't been enabled by its extension.
  bool preview_enabled_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PageActionImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_IMAGE_VIEW_H_
