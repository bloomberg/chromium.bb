// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/upgrade/upgrade_center.h"

namespace ios {
class ChromeBrowserState;
}

@protocol InfobarContainerConsumer;
@protocol LanguageSelectionHandler;
@protocol SigninPresenter;
@protocol SyncPresenter;
@class TabModel;

@interface InfobarContainerMediator : NSObject<UpgradeCenterClient>

// Designated initializer. None of the parameters are retained.
- (instancetype)initWithConsumer:(id<InfobarContainerConsumer>)consumer
                    browserState:(ios::ChromeBrowserState*)browserState
                        tabModel:(TabModel*)tabModel NS_DESIGNATED_INITIALIZER;
;

- (instancetype)init NS_UNAVAILABLE;

// The SyncPresenter delegate for this Mediator.
@property(nonatomic, weak) id<SyncPresenter> syncPresenter;

// The SigninPresenter delegate for this Mediator.
@property(nonatomic, weak) id<SigninPresenter> signinPresenter;

// The delegate that allows user to select a language from a list of languages.
@property(nonatomic, weak) id<LanguageSelectionHandler>
    languageSelectionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_MEDIATOR_H_
