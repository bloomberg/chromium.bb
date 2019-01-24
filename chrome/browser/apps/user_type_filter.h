// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_USER_TYPE_FILTER_H_
#define CHROME_BROWSER_APPS_USER_TYPE_FILTER_H_

#include <string>

class Profile;

namespace base {
class Value;
class ListValue;
}  // namespace base

namespace apps {

extern const char kKeyUserType[];

extern const char kUserTypeChild[];
extern const char kUserTypeGuest[];
extern const char kUserTypeManaged[];
extern const char kUserTypeSupervised[];
extern const char kUserTypeUnmanaged[];

// This filter is used to verify that testing profile |profile| matches user
// type filter defined in root Json element |json_root|. |json_root| should have
// the list of acceptable user types. Following values are valid: child, guest,
// managed, supervised, unmanaged. Primary use of this filter is to determine if
// particular default webapp or extension has to be installed for current user.
// Returns true if profile is accepted for this filter. |default_user_types| is
// optional and used to support transition for the extension based default apps.
// |app_id| is used for error logging purpose.
bool ProfileMatchJsonUserType(Profile* profile,
                              const std::string& app_id,
                              const base::Value* json_root,
                              const base::ListValue* default_user_types);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_USER_TYPE_FILTER_H_
