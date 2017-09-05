// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ADAPTOR_BROWSER_COMMANDS_ADAPTOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ADAPTOR_BROWSER_COMMANDS_ADAPTOR_H_

#include <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/commands/browser_commands.h"

// Browser commands to be used for components from the old architecture.
// TODO(crbug.com/740793): This class should be hooked with the new architecture
// way of handling commands. Once it is done, remove alerts
@interface BrowserCommandsAdaptor : NSObject<BrowserCommands>

// TODO(crbug.com/740793): Remove alert once this class is functional.
@property(nonatomic, strong) UIViewController* viewControllerForAlert;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ADAPTOR_BROWSER_COMMANDS_ADAPTOR_H_
