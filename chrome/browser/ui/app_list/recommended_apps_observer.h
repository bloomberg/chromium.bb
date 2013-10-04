// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_OBSERVER_H_
#define CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_OBSERVER_H_

namespace app_list {

// An interface for observing RecommendedApps change.
class RecommendedAppsObserver {
 public:
  // Invoked when RecommendedApps changed.
  virtual void OnRecommendedAppsChanged() = 0;

 protected:
  virtual ~RecommendedAppsObserver() {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_OBSERVER_H_
