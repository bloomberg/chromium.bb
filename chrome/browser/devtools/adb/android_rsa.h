// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_ADB_ANDROID_RSA_H_
#define CHROME_BROWSER_DEVTOOLS_ADB_ANDROID_RSA_H_

#include <string>

class Profile;

std::string AndroidRSAPublicKey(Profile* profile);

std::string AndroidRSASign(Profile* profile, const std::string& body);

#endif  // CHROME_BROWSER_DEVTOOLS_ADB_ANDROID_RSA_H_
