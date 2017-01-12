// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_APP_APP_DELEGATE_H_
#define IOS_CLEAN_CHROME_APP_APP_DELEGATE_H_

#import <UIKit/UIKit.h>

// The main delegate of the application.
// This class is intended solely to handle the numerous UIApplicationDelegate
// methods and route to other application objects as appropriate. While this
// object does create and own other objects, including the ApplicationState
// object, it shouldn't perform significant manipulation or configuration of
// them. Nor should any of the method implementations in this class contain
// significant logic or any specific feature support; that should all be
// implemented in subsidiary objects this class makes use of.
@interface AppDelegate : NSObject<UIApplicationDelegate>
// Do not add any public API to this class. Other code in Chrome shouldn't
// need to access instances of this class for any purpose.
@end

#endif  // IOS_CLEAN_CHROME_APP_APP_DELEGATE_H_
