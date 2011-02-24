// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_
#pragma once

#include "chrome/browser/extensions/image_loading_tracker.h"
#import "chrome/browser/ui/cocoa/location_bar/image_decoration.h"
#include "googleurl/src/gurl.h"

class ExtensionAction;
@class ExtensionActionContextMenu;
class LocationBarViewMac;
class Profile;
class TabContents;

// PageActionDecoration is used to display the icon for a given Page
// Action and notify the extension when the icon is clicked.

class PageActionDecoration : public ImageDecoration,
                             public ImageLoadingTracker::Observer,
                             public NotificationObserver {
 public:
  PageActionDecoration(LocationBarViewMac* owner,
                       Profile* profile,
                       ExtensionAction* page_action);
  virtual ~PageActionDecoration();

  ExtensionAction* page_action() { return page_action_; }
  int current_tab_id() { return current_tab_id_; }
  void set_preview_enabled(bool enabled) { preview_enabled_ = enabled; }
  bool preview_enabled() const { return preview_enabled_; }

  // Overridden from |ImageLoadingTracker::Observer|.
  virtual void OnImageLoaded(
      SkBitmap* image, ExtensionResource resource, int index);

  // Called to notify the Page Action that it should determine whether
  // to be visible or hidden. |contents| is the TabContents that is
  // active, |url| is the current page URL.
  void UpdateVisibility(TabContents* contents, const GURL& url);

  // Sets the tooltip for this Page Action image.
  void SetToolTip(NSString* tooltip);
  void SetToolTip(std::string tooltip);

  // Get the point where extension info bubbles should point within
  // the given decoration frame.
  NSPoint GetBubblePointInFrame(NSRect frame);

  // Overridden from |LocationBarDecoration|
  virtual CGFloat GetWidthForSpace(CGFloat width);
  virtual bool AcceptsMousePress();
  virtual bool OnMousePressed(NSRect frame);
  virtual NSString* GetToolTip();
  virtual NSMenu* GetMenu();

 protected:
  // For unit testing only.
  PageActionDecoration() : owner_(NULL),
                           profile_(NULL),
                           page_action_(NULL),
                           tracker_(this),
                           current_tab_id_(-1),
                           preview_enabled_(false) {}

 private:
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The location bar view that owns us.
  LocationBarViewMac* owner_;

  // The current profile (not owned by us).
  Profile* profile_;

  // The Page Action that this view represents. The Page Action is not
  // owned by us, it resides in the extension of this particular
  // profile.
  ExtensionAction* page_action_;

  // A cache of images the Page Actions might need to show, mapped by
  // path.
  typedef std::map<std::string, SkBitmap> PageActionMap;
  PageActionMap page_action_icons_;

  // The object that is waiting for the image loading to complete
  // asynchronously.
  ImageLoadingTracker tracker_;

  // The tab id we are currently showing the icon for.
  int current_tab_id_;

  // The URL we are currently showing the icon for.
  GURL current_url_;

  // The string to show for a tooltip.
  scoped_nsobject<NSString> tooltip_;

  // The context menu for the Page Action.
  scoped_nsobject<ExtensionActionContextMenu> menu_;

  // This is used for post-install visual feedback. The page_action
  // icon is briefly shown even if it hasn't been enabled by its
  // extension.
  bool preview_enabled_;

  // Used to register for notifications received by
  // NotificationObserver.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PageActionDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PAGE_ACTION_DECORATION_H_
