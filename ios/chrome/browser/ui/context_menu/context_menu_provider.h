// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_PROVIDER_H_

// A protocol implemented by a provider of labels and actions for a context
// menu.
@protocol ContextMenuProvider<NSObject>

// Returns a ContextMenuHolder with the titles and actions associated with
// each menu item. The "Cancel" item is automatically added when constructing
// the menu for presentation, therefore it should not be added to the list.
// Each ContextMenuProvider might have different requirements for the
// |contextDictionary|. On a web page |kContextLinkURLString| and
// |kContextImageURLString| might both be set whereas on a recently visited
// entry some other identifier will be passed on.
- (ContextMenuHolder*)contextMenuForDictionary:(NSDictionary*)context;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_PROVIDER_H_
