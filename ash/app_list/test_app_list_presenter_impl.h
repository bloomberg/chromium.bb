// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_APP_LIST_PRESENTER_IMPL_H_
#define ASH_APP_LIST_TEST_APP_LIST_PRESENTER_IMPL_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"

namespace ash {

// An app list presenter impl subclass that presents a no-op test app list view.
// Some tests rely on the presenter and delegate implementations' behavior.
class ASH_EXPORT TestAppListPresenterImpl
    : public app_list::AppListPresenterImpl {
 public:
  TestAppListPresenterImpl();
  ~TestAppListPresenterImpl() override;

  // Helpers that show or hide the app list and spin a run loop for unit tests.
  // Some tests synchronously check expectations after Show/Dismiss mojo calls.
  void ShowAndRunLoop(int64_t display_id);
  void DismissAndRunLoop();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppListPresenterImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_APP_LIST_PRESENTER_IMPL_H_
