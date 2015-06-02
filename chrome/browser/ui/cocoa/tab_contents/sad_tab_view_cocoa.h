// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_

#include "base/mac/scoped_nsobject.h"

#import <Cocoa/Cocoa.h>

namespace content {
class WebContents;
}

@class SadTabContainerView;

// A view that displays the "sad tab" (aka crash page).
@interface SadTabView : NSView {
 @private
  base::scoped_nsobject<SadTabContainerView> container_;
}

@property(readonly,nonatomic) NSButton* reloadButton;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_
