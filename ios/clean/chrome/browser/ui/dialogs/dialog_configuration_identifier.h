// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_CONFIGURATION_IDENTIFIER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_CONFIGURATION_IDENTIFIER_H_

#import <Foundation/Foundation.h>

// Identifier objects used to communicate interaction events that occur from UI
// elements corresponding to dialog configuration objects passed to
// DialogConsumers.
@interface DialogConfigurationIdentifier : NSObject<NSCopying>
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_CONFIGURATION_IDENTIFIER_H_
