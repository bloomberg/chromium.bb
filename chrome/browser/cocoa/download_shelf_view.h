// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_SHELF_VIEW_H_
#define CHROME_BROWSER_COCOA_SHELF_VIEW_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/animatable_view.h"

// A view that handles any special rendering for the download shelf, painting
// a gradient and managing a set of DownloadItemViews.

@interface DownloadShelfView : AnimatableView {
}
@end

#endif  // CHROME_BROWSER_COCOA_SHELF_VIEW_H_
