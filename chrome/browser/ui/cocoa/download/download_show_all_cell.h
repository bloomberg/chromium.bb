// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_SHOW_ALL_CELL_H_
#define CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_SHOW_ALL_CELL_H_
#pragma once

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/download/download_util.h"
#import "chrome/browser/ui/cocoa/gradient_button_cell.h"

// The cell of the "Show All" button on the download shelf.
@interface DownloadShowAllCell : GradientButtonCell<NSAnimationDelegate> {
 @private
  scoped_ptr<ui::ThemeProvider> themeProvider_;
}

@end

#endif  // CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_SHOW_ALL_CELL_H_
