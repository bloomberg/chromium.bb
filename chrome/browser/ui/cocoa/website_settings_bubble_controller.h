// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"

class TabContents;
class WebsiteSettingsUIBridge;

// This NSWindowController subclass manages the InfoBubbleWindow and view that
// are displayed when the user clicks the favicon or security lock icon.
@interface WebsiteSettingsBubbleController : BaseBubbleController {
 @private
  scoped_nsobject<NSView> contentView_;
  scoped_nsobject<NSTabView> tabView_;

  // Displays the web site identity.
  NSTextField* identityField_;

  // Display the identity status (e.g. verified, not verified).
  NSTextField* identityStatusField_;

  // The UI translates user actions to specific events and forwards them to the
  // |presenter_|. The |presenter_| handles these events and updates the UI.
  scoped_ptr<WebsiteSettings> presenter_;

  // Bridge which implements the WebsiteSettingsUI interface and forwards
  // methods on to this class.
  scoped_ptr<WebsiteSettingsUIBridge> bridge_;
}

// Designated initializer. The controller will release itself when the bubble
// is closed. |parentWindow| cannot be nil.
- (id)initWithParentWindow:(NSWindow*)parentWindow
   websiteSettingsUIBridge:(WebsiteSettingsUIBridge*)bridge;

@end

// Provides a bridge between the WebSettingsUI C++ interface and the Cocoa
// implementation in WebsiteSettingsBubbleController.
class WebsiteSettingsUIBridge : public WebsiteSettingsUI {
 public:
  WebsiteSettingsUIBridge();
  virtual ~WebsiteSettingsUIBridge();

  // Creates a |WebsiteSettingsBubbleController| and displays the UI. |parent|
  // contains the currently active window, |profile| contains the currently
  // active profile and |ssl| contains the |SSLStatus| of the connection to the
  // website in the currently active tab that is wrapped by the
  // |tab_contents|.
  static void Show(gfx::NativeWindow parent,
                   Profile* profile,
                   TabContents* tab_contents,
                   const GURL& url,
                   const content::SSLStatus& ssl);

  void set_bubble_controller(
      WebsiteSettingsBubbleController* bubble_controller);

  // WebsiteSettingsUI implementations.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) OVERRIDE;
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) OVERRIDE;
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) OVERRIDE;
  virtual void SetFirstVisit(const string16& first_visit) OVERRIDE;

 private:
  // The Cocoa controller for the bubble UI.
  WebsiteSettingsBubbleController* bubble_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsUIBridge);
};
