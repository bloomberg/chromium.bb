// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PAGE_INFO_PAGE_INFO_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PAGE_INFO_PAGE_INFO_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "chrome/browser/ui/cocoa/omnibox_decoration_bubble_controller.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "content/public/browser/web_contents_observer.h"

class PageInfoUIBridge;

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
@interface PageInfoBubbleController : OmniboxDecorationBubbleController {
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
  std::unique_ptr<PageInfo> presenter_;

  // Bridge which implements the PageInfoUI interface and forwards
  // methods on to this class.
  std::unique_ptr<PageInfoUIBridge> bridge_;
}

// Designated initializer. The controller will release itself when the bubble
// is closed. |parentWindow| cannot be nil. |webContents| may be nil for
// testing purposes.
- (id)initWithParentWindow:(NSWindow*)parentWindow
          pageInfoUIBridge:(PageInfoUIBridge*)bridge
               webContents:(content::WebContents*)webContents
                       url:(const GURL&)url;

// Return the default width of the window. It may be wider to fit the content.
// This may be overriden by a subclass for testing purposes.
- (CGFloat)defaultWindowWidth;

@end

// Provides a bridge between the PageInfoUI C++ interface and the Cocoa
// implementation in PageInfoBubbleController.
class PageInfoUIBridge : public content::WebContentsObserver,
                         public PageInfoUI {
 public:
  explicit PageInfoUIBridge(content::WebContents* web_contents);
  ~PageInfoUIBridge() override;

  // Creates a |PageInfoBubbleController| and displays the UI. |parent|
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

  void set_bubble_controller(PageInfoBubbleController* bubble_controller);

  // WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // PageInfoUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

 private:
  // The WebContents the bubble UI is attached to.
  content::WebContents* web_contents_;

  // The Cocoa controller for the bubble UI.
  PageInfoBubbleController* bubble_controller_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoUIBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_PAGE_INFO_PAGE_INFO_BUBBLE_CONTROLLER_H_
