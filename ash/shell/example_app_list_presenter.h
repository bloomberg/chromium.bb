// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_EXAMPLE_APP_LIST_PRESENTER_H_
#define ASH_SHELL_EXAMPLE_APP_LIST_PRESENTER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/app_list/presenter/app_list_presenter.mojom.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/presenter/test/test_app_list_view_delegate_factory.h"

namespace ash {
namespace shell {

// An example implementation of AppListPresenter used for ash_shell.
class ExampleAppListPresenter : public app_list::mojom::AppListPresenter {
 public:
  ExampleAppListPresenter();
  ~ExampleAppListPresenter() override;

  app_list::mojom::AppListPresenterPtr CreateInterfacePtrAndBind();

  // app_list::mojom::AppListPresenter:
  void Show(int64_t display_id) override;
  void Dismiss() override;
  void ToggleAppList(int64_t display_id) override;
  void StartVoiceInteractionSession() override;
  void ToggleVoiceInteractionSession() override;
  void UpdateYPositionAndOpacity(int new_y_position,
                                 float background_opacity) override;
  void EndDragFromShelf(app_list::mojom::AppListState app_list_state) override;
  void ProcessMouseWheelOffset(int offset) override;

 private:
  mojo::Binding<app_list::mojom::AppListPresenter> binding_;
  app_list::AppListPresenterImpl app_list_presenter_impl_;

  DISALLOW_COPY_AND_ASSIGN(ExampleAppListPresenter);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_EXAMPLE_APP_LIST_PRESENTER_H_
