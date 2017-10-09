// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test_app_list_presenter_impl.h"

#include <memory>

#include "ash/app_list/app_list_presenter_delegate_factory.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/app_list/presenter/test/test_app_list_view_delegate_factory.h"

namespace ash {

TestAppListPresenterImpl::TestAppListPresenterImpl()
    : app_list::AppListPresenterImpl(std::make_unique<
                                     AppListPresenterDelegateFactory>(
          std::make_unique<app_list::test::TestAppListViewDelegateFactory>())) {
  // Connect Ash's app list implementation to the presenter.
  app_list::mojom::AppListPtr app_list_ptr;
  Shell::Get()->app_list()->BindRequest(mojo::MakeRequest(&app_list_ptr));
  SetAppList(std::move(app_list_ptr));
}

TestAppListPresenterImpl::~TestAppListPresenterImpl() {}

void TestAppListPresenterImpl::ShowAndRunLoop(int64_t display_id) {
  Show(display_id);
  base::RunLoop().RunUntilIdle();
}

void TestAppListPresenterImpl::DismissAndRunLoop() {
  Dismiss();
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
