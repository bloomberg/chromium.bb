// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_icon_factory.h"
#import "chrome/browser/ui/cocoa/location_bar/image_decoration.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "url/gurl.h"

@class ExtensionActionContextMenuController;
class Browser;
class LocationBarViewMac;

namespace content {
class WebContents;
}

// PageActionDecoration is used to display the icon for a given Page
// Action and notify the extension when the icon is clicked.

class PageActionDecoration : public ImageDecoration,
                             public ExtensionActionIconFactory::Observer,
                             public content::NotificationObserver {
 public:
  PageActionDecoration(LocationBarViewMac* owner,
                       Browser* browser,
                       ExtensionAction* page_action);
  virtual ~PageActionDecoration();

  ExtensionAction* page_action() { return page_action_; }
  int current_tab_id() { return current_tab_id_; }
  void set_preview_enabled(bool enabled) { preview_enabled_ = enabled; }
  bool preview_enabled() const { return preview_enabled_; }

  // Overridden from |ExtensionActionIconFactory::Observer|.
  virtual void OnIconUpdated() OVERRIDE;

  // Called to notify the Page Action that it should determine whether
  // to be visible or hidden. |contents| is the WebContents that is
  // active, |url| is the current page URL.
  void UpdateVisibility(content::WebContents* contents, const GURL& url);

  // Sets the tooltip for this Page Action image.
  void SetToolTip(NSString* tooltip);
  void SetToolTip(std::string tooltip);

  // Overridden from |LocationBarDecoration|
  virtual CGFloat GetWidthForSpace(CGFloat width) OVERRIDE;
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame, NSPoint location) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;
  virtual NSMenu* GetMenu() OVERRIDE;
  virtual NSPoint GetBubblePointInFrame(NSRect frame) OVERRIDE;

  // Activate the page action in its default frame.
  void ActivatePageAction();

 private:
  // Activate the page action in the given |frame|.
  bool ActivatePageAction(NSRect frame);

  // Show the popup in the frame, with the given URL.
  void ShowPopup(const NSRect& frame, const GURL& popup_url);

  // Overridden from NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The location bar view that owns us.
  LocationBarViewMac* owner_;

  // The current browser (not owned by us).
  Browser* browser_;

  // The Page Action that this view represents. The Page Action is not
  // owned by us, it resides in the extension of this particular
  // profile.
  ExtensionAction* page_action_;


  // The object that will be used to get the page action icon for us.
  // It may load the icon asynchronously (in which case the initial icon
  // returned by the factory will be transparent), so we have to observe it for
  // updates to the icon.
  scoped_ptr<ExtensionActionIconFactory> icon_factory_;

  // The tab id we are currently showing the icon for.
  int current_tab_id_;

  // The URL we are currently showing the icon for.
  GURL current_url_;

  // The string to show for a tooltip.
  base::scoped_nsobject<NSString> tooltip_;

  // The context menu controller for the Page Action.
  base::scoped_nsobject<
      ExtensionActionContextMenuController> contextMenuController_;

  // This is used for post-install visual feedback. The page_action
  // icon is briefly shown even if it hasn't been enabled by its
  // extension.
  bool preview_enabled_;

  // Used to register for notifications received by
  // NotificationObserver.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PageActionDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_
