// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_shower_delegate_factory.h"

#include "ash/app_list/app_list_shower_delegate.h"
#include "ash/app_list/app_list_view_delegate_factory.h"
#include "base/memory/ptr_util.h"

namespace ash {

AppListShowerDelegateFactory::AppListShowerDelegateFactory(
    std::unique_ptr<AppListViewDelegateFactory> view_delegate_factory)
    : view_delegate_factory_(std::move(view_delegate_factory)) {}

AppListShowerDelegateFactory::~AppListShowerDelegateFactory() {}

std::unique_ptr<app_list::AppListShowerDelegate>
AppListShowerDelegateFactory::GetDelegate(app_list::AppListShower* shower) {
  return base::WrapUnique(
      new AppListShowerDelegate(shower, view_delegate_factory_.get()));
}

}  // namespace ash
