// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_PROVIDER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_PROVIDER_H_

#import <Foundation/Foundation.h>

@class CRUContextMenuHolder;

// A protocol implemented by a provider of labels and actions for a context
// menu.
@protocol CRWContextMenuProvider

// Returns a CRUContextMenuHolder with the titles and actions associated with
// each menu item. The "Cancel" item is automatically added when constructing
// the menu for presentation, therefore it should not be added to the list.
// Each CRWContextMenuProvider might have different requirements for the
// |contextDictionary|. On a web page |kContextLinkURLString| and
// |kContextImageURLString| might both be set whereas on a recently visited
// entry some other identifier will be passed on.
- (CRUContextMenuHolder*)contextMenuForDictionary:(NSDictionary*)context;

@end

namespace web {

// Keys for the contextDictionary.

// The menu title (as an NSString).
extern NSString* const kContextTitle;
// The url spec (as an NSString) of the the inner-most wrapping anchor tag to
// the current element.
extern NSString* const kContextLinkURLString;
// The referrer policy to use when opening the link.
extern NSString* const kContextLinkReferrerPolicy;
// The url spec (as an NSString) for the image of the current element.
extern NSString* const kContextImageURLString;

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_PROVIDER_H_
