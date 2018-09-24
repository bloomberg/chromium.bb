// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BUBBLE_ANCHOR_HELPER_H_
#define CHROME_BROWSER_UI_COCOA_BUBBLE_ANCHOR_HELPER_H_

#import <Foundation/Foundation.h>

class Browser;

// Returns true if |browser| has a visible location bar.
bool HasVisibleLocationBarForBrowser(Browser* browser);

#endif  // CHROME_BROWSER_UI_COCOA_BUBBLE_ANCHOR_HELPER_H_
