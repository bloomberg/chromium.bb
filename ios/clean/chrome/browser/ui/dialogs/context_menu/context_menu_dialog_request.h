// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_CONTEXT_MENU_CONTEXT_MENU_DIALOG_REQUEST_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_CONTEXT_MENU_CONTEXT_MENU_DIALOG_REQUEST_H_

#import <Foundation/Foundation.h>

#include "base/strings/string16.h"

namespace web {
struct ContextMenuParams;
}

class GURL;

// A container object encapsulating all the state necessary to support a
// ContextMenuCoordinator.
@interface ContextMenuDialogRequest : NSObject

// ContextMenuContextImpls must be initialized with |params|.
+ (instancetype)requestWithParams:(const web::ContextMenuParams&)params;
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

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_CONTEXT_MENU_CONTEXT_MENU_DIALOG_REQUEST_H_
