// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The local-auth module allows for user authentication in the case when
// on-line authentication is not possible (e.g. there is no network
// connection).

#ifndef CHROME_BROWSER_SIGNIN_LOCAL_AUTH_H_
#define CHROME_BROWSER_SIGNIN_LOCAL_AUTH_H_

#include <string>

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chrome {

void RegisterLocalAuthPrefs(user_prefs::PrefRegistrySyncable* registry);

void SetLocalAuthCredentials(Profile* profile,
                             const std::string& username,
                             const std::string& password);

bool ValidateLocalAuthCredentials(Profile* profile,
                                  const std::string& username,
                                  const std::string& password);

}  // namespace chrome

#endif  // CHROME_BROWSER_SIGNIN_LOCAL_AUTH_H_
