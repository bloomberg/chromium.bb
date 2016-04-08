// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_SHOWER_DELEGATE_FACTORY_H_
#define ASH_APP_LIST_APP_LIST_SHOWER_DELEGATE_FACTORY_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/app_list/shower/app_list_shower_delegate_factory.h"

namespace app_list {
class AppListShower;
}

namespace ash {

class AppListViewDelegateFactory;

class ASH_EXPORT AppListShowerDelegateFactory
    : public app_list::AppListShowerDelegateFactory {
 public:
  explicit AppListShowerDelegateFactory(
      std::unique_ptr<AppListViewDelegateFactory> view_delegate_factory);
  ~AppListShowerDelegateFactory() override;

  std::unique_ptr<app_list::AppListShowerDelegate> GetDelegate(
      app_list::AppListShower* shower) override;

 private:
  std::unique_ptr<AppListViewDelegateFactory> view_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListShowerDelegateFactory);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_SHOWER_DELEGATE_FACTORY_H_
