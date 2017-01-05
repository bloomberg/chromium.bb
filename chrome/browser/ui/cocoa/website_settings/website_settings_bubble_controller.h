// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "chrome/browser/ui/cocoa/omnibox_decoration_bubble_controller.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "content/public/browser/web_contents_observer.h"

class WebsiteSettingsUIBridge;

namespace content {
class WebContents;
}

namespace net {
class X509Certificate;
}

namespace security_state {
struct SecurityInfo;
}  // namespace security_state

// This NSWindowController subclass manages the InfoBubbleWindow and view that
// are displayed when the user clicks the favicon or security lock icon.
//
// TODO(palmer): Normalize all WebsiteSettings*, SiteSettings*, PageInfo*, et c.
// to OriginInfo*.
@interface WebsiteSettingsBubbleController : OmniboxDecorationBubbleController {
 @private
  content::WebContents* webContents_;

  base::scoped_nsobject<NSView> contentView_;

  // The main content view for the Permissions tab.
  NSView* securitySectionView_;

  // Displays the short security summary for the page
  // (private/not private/etc.).
  NSTextField* securitySummaryField_;

  // Displays a longer explanation of the page's security state, and how the
  // user should treat it.
  NSTextField* securityDetailsField_;

  // The link button for opening a Chrome Help Center page explaining connection
  // security.
  NSButton* connectionHelpButton_;

  // URL of the page for which the bubble is shown.
  GURL url_;

  // Displays a paragraph to accompany the reset decisions button, explaining
  // that the user has made a decision to trust an invalid security certificate
  // for the current site.
  // This field only shows when there is an acrive certificate exception.
  NSTextField* resetDecisionsField_;

  // The link button for revoking certificate decisions.
  // This link only shows when there is an acrive certificate exception.
  NSButton* resetDecisionsButton_;

  // The server certificate from the identity info. This should always be
  // non-null on a cryptographic connection, and null otherwise.
  scoped_refptr<net::X509Certificate> certificate_;

  // Separator line.
  NSView* separatorAfterSecuritySection_;

  // Container for the site settings section.
  NSView* siteSettingsSectionView_;

  // Container for cookies info in the site settings section.
  NSView* cookiesView_;

  // Container for permission info in the site settings section.
  NSView* permissionsView_;

  // Whether the permissionView_ shows anything.
  BOOL permissionsPresent_;

  // The link button for showing site settings.
  NSButton* siteSettingsButton_;

  // The UI translates user actions to specific events and forwards them to the
  // |presenter_|. The |presenter_| handles these events and updates the UI.
  std::unique_ptr<WebsiteSettings> presenter_;

  // Bridge which implements the WebsiteSettingsUI interface and forwards
  // methods on to this class.
  std::unique_ptr<WebsiteSettingsUIBridge> bridge_;
}

// Designated initializer. The controller will release itself when the bubble
// is closed. |parentWindow| cannot be nil. |webContents| may be nil for
// testing purposes.
- (id)initWithParentWindow:(NSWindow*)parentWindow
    websiteSettingsUIBridge:(WebsiteSettingsUIBridge*)bridge
                webContents:(content::WebContents*)webContents
                        url:(const GURL&)url;

// Return the default width of the window. It may be wider to fit the content.
// This may be overriden by a subclass for testing purposes.
- (CGFloat)defaultWindowWidth;

@end

// Provides a bridge between the WebSettingsUI C++ interface and the Cocoa
// implementation in WebsiteSettingsBubbleController.
class WebsiteSettingsUIBridge : public content::WebContentsObserver,
                                public WebsiteSettingsUI {
 public:
  explicit WebsiteSettingsUIBridge(content::WebContents* web_contents);
  ~WebsiteSettingsUIBridge() override;

  // Creates a |WebsiteSettingsBubbleController| and displays the UI. |parent|
  // is the currently active window. |profile| points to the currently active
  // profile. |web_contents| points to the WebContents that wraps the currently
  // active tab. |virtual_url| is the virtual GURL of the currently active
  // tab. |security_info| is the |security_state::SecurityInfo| of the
  // connection to the website in the currently active tab.
  static void Show(gfx::NativeWindow parent,
                   Profile* profile,
                   content::WebContents* web_contents,
                   const GURL& virtual_url,
                   const security_state::SecurityInfo& security_info);

  void set_bubble_controller(
      WebsiteSettingsBubbleController* bubble_controller);

  // WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // WebsiteSettingsUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

 private:
  // The WebContents the bubble UI is attached to.
  content::WebContents* web_contents_;

  // The Cocoa controller for the bubble UI.
  WebsiteSettingsBubbleController* bubble_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsUIBridge);
};
