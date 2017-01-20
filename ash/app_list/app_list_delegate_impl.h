// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_DELEGATE_IMPL_H_
#define ASH_APP_LIST_APP_LIST_DELEGATE_IMPL_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/app_list/presenter/app_list_delegate.h"

namespace ash {

class ASH_EXPORT AppListDelegateImpl : public app_list::AppListDelegate {
 public:
  explicit AppListDelegateImpl();
  ~AppListDelegateImpl() override;

 private:
  // app_list::AppListDelegate:
  void OnAppListVisibilityChanged(bool visible, int64_t display_id) override;

  DISALLOW_COPY_AND_ASSIGN(AppListDelegateImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_DELEGATE_IMPL_H_
