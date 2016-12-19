// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_PRERENDER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PRERENDER_DELEGATE_H_

// A protocol for accessing and commanding prerenderer.
@protocol PrerenderDelegate
@optional
// Discard prerenderer (e.g. when a dialog was suppressed).
- (void)discardPrerender;
// Is pre-render tab.
- (BOOL)isPrerenderTab;
@end

#endif  // IOS_CHROME_BROWSER_UI_PRERENDER_DELEGATE_H_
