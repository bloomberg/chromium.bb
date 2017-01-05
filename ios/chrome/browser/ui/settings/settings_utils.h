// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_UTILS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/procedural_block_types.h"

namespace ios_internal_settings {

// Returns a ProceduralBlockWithURL that sends a chrome command up the
// |responder|'s responder chain that dismisses settings and opens url
// (parameter to the block) in a new tab.
ProceduralBlockWithURL BlockToOpenURL(UIResponder* responder);

}  // namespace ios_internal_settings

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_UTILS_H_
