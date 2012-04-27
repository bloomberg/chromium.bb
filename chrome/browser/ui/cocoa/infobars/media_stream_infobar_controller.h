// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_MEDIA_STREAM_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_MEDIA_STREAM_INFOBAR_CONTROLLER_H_

#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"

#import "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

class InfoBarTabHelper;
class MediaStreamDevicesMenuModel;
class MediaStreamInfoBarDelegate;
@class NSButton;
@class NSPopUpButton;
@class NSTextField;

// The class handles the Mac OS X specifics for the MediaStream infobar allowing
// or denying capture device usage.
@interface MediaStreamInfoBarController : InfoBarController {
 @protected
  // Contains all text displayed on the infobar.
  scoped_nsobject<NSTextField> label1_;
  // Contains a list of video and audio capture devices.
  scoped_nsobject<NSPopUpButton> deviceMenu_;
  // Menu model for MediaStream capture devices.
  scoped_ptr<MediaStreamDevicesMenuModel> deviceMenuModel_;
}

- (id)initWithDelegate:(MediaStreamInfoBarDelegate*)delegate
                 owner:(InfoBarTabHelper*)owner;

// Called when the selection in the device menu has changed.
- (IBAction)deviceMenuChanged:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_MEDIA_STREAM_INFOBAR_CONTROLLER_H_
