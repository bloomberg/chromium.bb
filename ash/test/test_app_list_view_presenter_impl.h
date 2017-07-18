// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_APP_LIST_VIEW_PRESENTER_IMPL_H_
#define ASH_TEST_TEST_APP_LIST_VIEW_PRESENTER_IMPL_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"

namespace ash {
namespace test {

// An app list presenter impl subclass that presents a no-op test app list view.
// Some tests rely on the presenter and delegate implementations' behavior.
class TestAppListViewPresenterImpl : public app_list::AppListPresenterImpl {
 public:
  TestAppListViewPresenterImpl();
  ~TestAppListViewPresenterImpl() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppListViewPresenterImpl);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_APP_LIST_VIEW_PRESENTER_IMPL_H_
