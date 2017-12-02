// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_

#include <memory>
#include <string>

#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"

class AppListControllerDelegate;
class Profile;

namespace base {
class Time;
}
namespace app_list {

class AppResult : public SearchResult,
                  public AppContextMenuDelegate {
 public:
  ~AppResult() override;

  void UpdateFromLastLaunchedOrInstalledTime(const base::Time& current_time,
                                             const base::Time& old_time);

  // Marked const in order to be able to use in derived class in const methods.
  Profile* profile() const { return profile_; }

  const std::string& app_id() const { return app_id_; }

 protected:
  AppResult(Profile* profile,
            const std::string& app_id,
            AppListControllerDelegate* controller,
            bool is_recommendation);

  // Marked const in order to be able to use in derived class in const methods.
  AppListControllerDelegate* controller() const {
    return controller_;
  }

 private:
  Profile* profile_;
  const std::string app_id_;
  AppListControllerDelegate* controller_;

  DISALLOW_COPY_AND_ASSIGN(AppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_
