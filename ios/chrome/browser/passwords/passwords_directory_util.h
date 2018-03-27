// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORDS_DIRECTORY_UTIL_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORDS_DIRECTORY_UTIL_H_

#import <Foundation/Foundation.h>

// Returns the URL to the passwords directory.
NSURL* GetPasswordsDirectoryURL();

// Asynchronously deletes the temporary passwords directory.
void DeletePasswordsDirectory();

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORDS_DIRECTORY_UTIL_H_
