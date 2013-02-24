// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CHROME_TO_MOBILE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_CHROME_TO_MOBILE_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
@class ChromeToMobileBubbleController;
class ChromeToMobileService;

namespace base {
class DictionaryValue;
}

namespace ui {
class Animation;
class ThrobAnimation;
}

// Simple class to watch for tab creation/destruction and close the bubble.
// Bridge between Chrome-style notifications and ObjC-style notifications.
// This class also observes ChromeToMobileService for the bubble.
class ChromeToMobileBubbleNotificationBridge
    : public content::NotificationObserver,
      public ChromeToMobileService::Observer,
      public base::SupportsWeakPtr<ChromeToMobileBubbleNotificationBridge> {
 public:
  ChromeToMobileBubbleNotificationBridge(
      ChromeToMobileBubbleController* controller,
      SEL selector);
  virtual ~ChromeToMobileBubbleNotificationBridge() {}

  // content::NotificationObserver override:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ChromeToMobileService::Observer overrides:
  virtual void SnapshotGenerated(const base::FilePath& path,
                                 int64 bytes) OVERRIDE;
  virtual void OnSendComplete(bool success) OVERRIDE;

 private:
  content::NotificationRegistrar registrar_;
  ChromeToMobileBubbleController* controller_;  // weak; owns us.
  SEL selector_;  // SEL sent to controller_ on notification.
};

// Controller for the Chrome To Mobile bubble, which pops up when clicking on
// mobile device icon between the omnibox URL and bookmark star. This bubble
// allows page URLs and MHTML snapshots to be sent to registered mobile devices.
@interface ChromeToMobileBubbleController
    : BaseBubbleController<NSAnimationDelegate> {
 @private
  // The bubble title: "Send this page to[:| <Device Name>.]".
  IBOutlet NSTextField* title_;

  // The radio group shown for multiple registered devices.
  IBOutlet NSMatrix* mobileRadioGroup_;

  // The box containing views: |sendCopy_|, |cancel_|, |send_|, and |error_|.
  IBOutlet NSBox* lowerBox_;

  // The checkbox to send an offline copy.
  IBOutlet NSButton* sendCopy_;

  // The cancel and send buttons.
  IBOutlet NSButton* cancel_;
  IBOutlet NSButton* send_;

  // The error message shown if sending the page fails.
  IBOutlet NSTextField* error_;

  // A bridge used to observe Chrome and Service changes.
  scoped_ptr<ChromeToMobileBubbleNotificationBridge> bridge_;

  // The browser that opened this bubble.
  Browser* browser_;

  // The Chrome To Mobile service associated with this bubble.
  ChromeToMobileService* service_;

  // The file path for the MHTML page snapshot.
  base::FilePath snapshotPath_;

  // An animation used to cycle through the "Sending..." status messages.
  scoped_nsobject<NSAnimation> progressAnimation_;
}

// The owner of this object is responsible for showing the bubble. It is not
// shown by the init routine. The window closes automatically on deallocation.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                   browser:(Browser*)browser;

// Actions for buttons in the dialog.
- (IBAction)learn:(id)sender;
- (IBAction)send:(id)sender;
- (IBAction)cancel:(id)sender;

// Update the bubble to reflect the generated snapshot.
- (void)snapshotGenerated:(const base::FilePath&)path
                    bytes:(int64)bytes;

// Update the bubble to reflect the completed send.
- (void)onSendComplete:(bool)success;

@end

@interface ChromeToMobileBubbleController (JustForTesting)

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   service:(ChromeToMobileService*)service;
- (void)setSendCopy:(bool)sendCopy;
- (ChromeToMobileBubbleNotificationBridge*)bridge;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CHROME_TO_MOBILE_BUBBLE_CONTROLLER_H_
