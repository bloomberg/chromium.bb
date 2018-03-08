// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/app_list_test_helper.h"

#include <utility>

#include "ash/app_list/presenter/app_list.h"
#include "ash/app_list/presenter/app_list_presenter_impl.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/views/app_list_view.h"

namespace ash {

AppListTestHelper::AppListTestHelper() {
  // The app list is ready after Shell is created.
  app_list_ = Shell::Get()->app_list();
  DCHECK(app_list_);

  // Use a new app list presenter for each test.
  app_list_presenter_service_ =
      std::make_unique<app_list::test::TestAppListPresenter>();
  app_list_->SetAppListPresenter(
      app_list_presenter_service_->CreateInterfacePtrAndBind());

  // Pretend that |app_list_| and |app_list_presenter_service_| are in different
  // processes.
  app_list::mojom::AppListPtr app_list_ptr;
  Shell::Get()->app_list()->BindRequest(mojo::MakeRequest(&app_list_ptr));
  app_list_presenter_service_->presenter()->SetAppList(std::move(app_list_ptr));
}

AppListTestHelper::~AppListTestHelper() = default;

void AppListTestHelper::WaitUntilIdle() {
  app_list_->FlushForTesting();
  app_list_presenter_service_->FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

void AppListTestHelper::ShowAndRunLoop(uint64_t display_id) {
  ShowAndRunLoop(display_id, app_list::AppListShowSource::kSearchKey);
}

void AppListTestHelper::ShowAndRunLoop(
    uint64_t display_id,
    app_list::AppListShowSource show_source) {
  DCHECK(app_list_);
  app_list_->Show(display_id, show_source);
  WaitUntilIdle();
}

void AppListTestHelper::DismissAndRunLoop() {
  DCHECK(app_list_);
  app_list_->Dismiss();
  WaitUntilIdle();
}

void AppListTestHelper::ToggleAndRunLoop(uint64_t display_id) {
  ToggleAndRunLoop(display_id, app_list::AppListShowSource::kSearchKey);
}

void AppListTestHelper::ToggleAndRunLoop(
    uint64_t display_id,
    app_list::AppListShowSource show_source) {
  DCHECK(app_list_);
  app_list_->ToggleAppList(display_id, show_source);
  WaitUntilIdle();
}

void AppListTestHelper::CheckVisibility(bool visible) {
  DCHECK(app_list_);
  EXPECT_EQ(visible, app_list_->IsVisible());
  EXPECT_EQ(visible, app_list_->GetTargetVisibility());
}

void AppListTestHelper::CheckVoiceSessionCount(size_t count) {
  DCHECK(app_list_presenter_service_);
  EXPECT_EQ(count, app_list_presenter_service_->voice_session_count());
}

void AppListTestHelper::CheckState(app_list::AppListViewState state) {
  DCHECK(app_list_presenter_service_);
  EXPECT_EQ(state, app_list_presenter_service_->GetAppListViewState());
}

app_list::AppListView* AppListTestHelper::GetAppListView() {
  return app_list_presenter_service_->presenter()->GetView();
}

}  // namespace ash
