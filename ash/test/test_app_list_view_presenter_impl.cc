// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_app_list_view_presenter_impl.h"

#include "ash/app_list/app_list_presenter_delegate_factory.h"
#include "base/memory/ptr_util.h"
#include "ui/app_list/presenter/test/test_app_list_view_delegate_factory.h"

namespace ash {
namespace test {

TestAppListViewPresenterImpl::TestAppListViewPresenterImpl()
    : app_list::AppListPresenterImpl(base::MakeUnique<
                                     AppListPresenterDelegateFactory>(
          base::MakeUnique<app_list::test::TestAppListViewDelegateFactory>())) {
}

TestAppListViewPresenterImpl::~TestAppListViewPresenterImpl() {}

}  // namespace test
}  // namespace ash
