// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONTEXT_IMPL_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONTEXT_IMPL_H_

#include "base/strings/string16.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_context.h"

namespace web {
struct ContextMenuParams;
}

class GURL;

// Context object used to populate the context menu UI and to handle commands
// from that UI.
@interface ContextMenuContextImpl : ContextMenuContext

// ContextMenuContextImpls must be initialized with |params|.
- (instancetype)initWithParams:(const web::ContextMenuParams&)params
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The title to use for the menu.
@property(nonatomic, readonly, strong) NSString* menuTitle;

// If the context menu was triggered by long-pressing a link, |linkURL| will be
// that link's URL.
@property(nonatomic, readonly) const GURL& linkURL;

// If the context menu was triggered by long-pressing an image, |imageURL| will
// be the URL for that image.
@property(nonatomic, readonly) const GURL& imageURL;

// If the context menu was triggered by long-pressing a JavaScript link,
// |script| will be the script for that link.
@property(nonatomic, readonly) const base::string16& script;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONTEXT_IMPL_H_
