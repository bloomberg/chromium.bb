// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_PASSWORD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_PASSWORD_COORDINATOR_H_

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"

class IOSChromeSavePasswordInfoBarDelegate;

// Coordinator that creates and manages the PasswordInfobar.
@interface InfobarPasswordCoordinator : InfobarCoordinator

- (instancetype)initWithInfoBarDelegate:(IOSChromeSavePasswordInfoBarDelegate*)
                                            passwordInfoBarDelegate
                                   type:(InfobarType)infobarType
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_PASSWORD_COORDINATOR_H_
