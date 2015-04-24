// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_BUNDLE_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_BUNDLE_UTIL_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/extensions/bundle_installer.h"

// For each item in |items|, adds a line containing the item's icon and title
// to |items_field|. Returns the required height for |items_field| in pixels.
CGFloat PopulateBundleItemsList(
    const extensions::BundleInstaller::ItemList& items,
    NSView* items_field);

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_BUNDLE_UTIL_H_
