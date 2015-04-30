// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#import "chrome/browser/ui/cocoa/location_bar/image_decoration.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"

class Browser;
class ExtensionAction;
class ExtensionActionViewController;
class LocationBarViewMac;
@class MenuController;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

// PageActionDecoration is used to display the icon for a given Page
// Action and notify the extension when the icon is clicked.

class PageActionDecoration : public ImageDecoration,
                             public ToolbarActionViewDelegate {
 public:
  PageActionDecoration(LocationBarViewMac* owner,
                       Browser* browser,
                       ExtensionAction* page_action);
  ~PageActionDecoration() override;

  void set_preview_enabled(bool enabled) { preview_enabled_ = enabled; }
  bool preview_enabled() const { return preview_enabled_; }

  // Returns the extension associated with this decoration.
  const extensions::Extension* GetExtension();

  // Returns the page action associated with this decoration.
  ExtensionAction* GetPageAction();

  // Called to notify the Page Action that it should determine whether
  // to be visible or hidden. |contents| is the WebContents that is
  // active, |url| is the current page URL.
  void UpdateVisibility(content::WebContents* contents);

  // Overridden from |LocationBarDecoration|
  CGFloat GetWidthForSpace(CGFloat width) override;
  bool AcceptsMousePress() override;
  bool OnMousePressed(NSRect frame, NSPoint location) override;
  NSString* GetToolTip() override;
  NSMenu* GetMenu() override;
  NSPoint GetBubblePointInFrame(NSRect frame) override;

  // Activates the page action in its default frame, and, if |grant_active_tab|
  // is true, grants active tab permission to the extension. Returns true if
  // a popup was shown.
  bool ActivatePageAction(bool grant_active_tab);

 private:
  // Sets the tooltip for this Page Action image.
  void SetToolTip(const base::string16& tooltip);

  // Overridden from ToolbarActionViewDelegate:
  content::WebContents* GetCurrentWebContents() const override;
  bool IsMenuRunning() const override;
  void UpdateState() override;

  // The location bar view that owns us.
  LocationBarViewMac* owner_;

  // The view controller for this page action.
  scoped_ptr<ExtensionActionViewController> viewController_;

  // The string to show for a tooltip.
  base::scoped_nsobject<NSString> tooltip_;

  // The context menu controller for the Page Action.
  base::scoped_nsobject<MenuController> contextMenuController_;

  // This is used for post-install visual feedback. The page_action
  // icon is briefly shown even if it hasn't been enabled by its
  // extension.
  bool preview_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PageActionDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_
