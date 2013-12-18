// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_CHROME_APP_LIST_TEST_SUPPORT_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_CHROME_APP_LIST_TEST_SUPPORT_H_

namespace app_list {
class AppListModel;
}

class AppListService;
class Profile;

namespace test {

// Gets the model keyed to the profile currently associated with |service|.
app_list::AppListModel* GetAppListModel(AppListService* service);

// Gets the app list service for the desktop type currently being tested.
AppListService* GetAppListService();

// Simulate a Chrome sign-in for the given |profile|.
void SigninProfile(Profile* profile);

}  // namespace test

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_CHROME_APP_LIST_TEST_SUPPORT_H_
