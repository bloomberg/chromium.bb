// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_FACTORY_H_
#define ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_FACTORY_H_

#include "ash/ash_export.h"

namespace app_list {
class AppListViewDelegate;
}

namespace ash {

class ASH_EXPORT AppListViewDelegateFactory {
 public:
  virtual ~AppListViewDelegateFactory() {}

  virtual app_list::AppListViewDelegate* GetDelegate() = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_FACTORY_H_
