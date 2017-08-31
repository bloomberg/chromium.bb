// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_UTILS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/procedural_block_types.h"

@protocol ApplicationCommands;

// Returns a ProceduralBlockWithURL that uses the dispatcher and opens url
// (parameter to the block) in a new tab.
ProceduralBlockWithURL BlockToOpenURL(UIResponder* responder,
                                      id<ApplicationCommands> dispatcher);

namespace ios_internal_settings {
// Returns a ProceduralBlockWithURL that uses the dispatcher and opens url
// (parameter to the block) in a new tab. Forwards to ::BlockToOpenURL.
// TODO(crbug.com/677941): remove this declaration once ios_internal code
// has been fixed to use the function in the global namespace instead.
ProceduralBlockWithURL BlockToOpenURL(UIResponder* responder,
                                      id<ApplicationCommands> dispatcher);
}  // namespace ios_internal_settings

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_UTILS_H_
