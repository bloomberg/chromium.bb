// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_
#define ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_

#include <memory>

#include "ash/app_list/model/app_list_view_state.h"
#include "ash/app_list/presenter/app_list.h"
#include "ash/app_list/presenter/test/test_app_list_presenter.h"
#include "ui/app_list/app_list_constants.h"

namespace app_list {
class AppListView;
}  // namespace app_list

namespace ash {

class AppListTestHelper {
 public:
  AppListTestHelper();
  ~AppListTestHelper();

  // Show the app list in |display_id|, and wait until animation and mojo calls
  // finish.
  // Note: we usually don't care about the show source in tests.
  void ShowAndRunLoop(uint64_t display_id);

  // Show the app list in |display_id| triggered with |show_source|, and wait
  // until animation and mojo calls finish.
  void ShowAndRunLoop(uint64_t display_id,
                      app_list::AppListShowSource show_source);

  // Dismiss the app list, and wait until animation and mojo calls finish.
  void DismissAndRunLoop();

  // Toggle the app list in |display_id|, and wait until animation and mojo
  // calls finish.
  // Note: we usually don't care about the show source in tests.
  void ToggleAndRunLoop(uint64_t display_id);

  // Toggle the app list in |display_id| triggered with |show_source|, and wait
  // until animation and mojo calls finish.
  void ToggleAndRunLoop(uint64_t display_id,
                        app_list::AppListShowSource show_source);

  // Check the visibility value of the app list and its target.
  // Fails in tests if either one doesn't match |visible|,.
  void CheckVisibility(bool visible);

  // Check the accumulated voice session count.
  void CheckVoiceSessionCount(size_t count);

  // Check the current app list view state.
  void CheckState(app_list::AppListViewState state);

  // Run all pending in message loop and flush all mojo calls.
  void WaitUntilIdle();

  app_list::AppListView* GetAppListView();

 private:
  app_list::AppList* app_list_ = nullptr;
  std::unique_ptr<app_list::test::TestAppListPresenter>
      app_list_presenter_service_;

  DISALLOW_COPY_AND_ASSIGN(AppListTestHelper);
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_
