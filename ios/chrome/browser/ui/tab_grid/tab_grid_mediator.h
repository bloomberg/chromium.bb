// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class TabModel;

// Mediates between model layer and tab grid UI layer.
@interface TabGridMediator : NSObject
@property(nonatomic, weak) TabModel* regularTabModel;
@property(nonatomic, weak) TabModel* incognitoTabModel;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_
