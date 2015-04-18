// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_DEVICE_PERMISSIONS_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_DEVICE_PERMISSIONS_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "extensions/browser/api/device_permissions_prompt.h"

class DevicePermissionsDialogController;

// Displays the device permissions prompt, and notifies the
// DevicePermissionsDialogController of selected devices.
@interface DevicePermissionsViewController
    : NSViewController<NSTableViewDataSource, NSTableViewDelegate> {
  IBOutlet NSTextField* titleField_;
  IBOutlet NSTextField* promptField_;
  IBOutlet NSButton* cancelButton_;
  IBOutlet NSButton* okButton_;
  IBOutlet NSTableView* tableView_;
  IBOutlet NSTableColumn* deviceNameColumn_;
  IBOutlet NSTableColumn* serialNumberColumn_;
  IBOutlet NSScrollView* scrollView_;

  DevicePermissionsDialogController* controller_;  // weak
  scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt_;
}

- (id)
    initWithController:(DevicePermissionsDialogController*)controller
                prompt:
                    (scoped_refptr<extensions::DevicePermissionsPrompt::Prompt>)
                        prompt;
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;
- (void)devicesChanged;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_DEVICE_PERMISSIONS_VIEW_CONTROLLER_H_
