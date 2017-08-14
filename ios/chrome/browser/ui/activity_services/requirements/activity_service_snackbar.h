// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_SNACKBAR_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_SNACKBAR_H_

// ActivityServiceSnackback contains methods for showing MDC snackbars.
@protocol ActivityServiceSnackbar

// Asks the implementor to show a snackbar with the given |message|.
- (void)showSnackbar:(NSString*)message;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_SNACKBAR_H_
