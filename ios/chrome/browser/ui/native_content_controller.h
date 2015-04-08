// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NATIVE_CONTENT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NATIVE_CONTENT_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state/crw_native_content.h"
#include "url/gurl.h"

@class UIView;

// Abstract base class for controllers that implement the behavior for native
// views that are presented inside the web content area. Automatically removes
// |view| from the view hierarchy when it is destroyed. Subclasses are
// responsible for setting the view (usually through loading a nib) and the
// page title.
@interface NativeContentController : NSObject<CRWNativeContent> {
 @protected
  UIView* _view;  // Top-level view.
  NSString* _title;
  GURL _url;
}

@property(nonatomic, retain) IBOutlet UIView* view;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, readonly, assign) const GURL& url;

// Initializer that attempts to load the nib specified in |nibName| for
// |url|, which may be nil.
- (instancetype)initWithNibName:(NSString*)nibName url:(const GURL&)url;

// Initializer with the |url| to be loaded.
- (instancetype)initWithURL:(const GURL&)url;

// Called when memory is low and this is not the foreground tab. Release
// anything (such as views) that can be easily re-created to free up RAM.
// Subclasses that override this method should always call
// [super handleLowMemory].
- (void)handleLowMemory;

@end

#endif  // IOS_CHROME_BROWSER_UI_NATIVE_CONTENT_CONTROLLER_H_
