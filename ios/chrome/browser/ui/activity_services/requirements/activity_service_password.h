// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_PASSWORD_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_PASSWORD_H_

@class PasswordController;

// ActivityServicePassword contains methods related to password autofill.
@protocol ActivityServicePassword

// Returns the PasswordController for the current active WebState.
- (PasswordController*)currentPasswordController;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_PASSWORD_H_
