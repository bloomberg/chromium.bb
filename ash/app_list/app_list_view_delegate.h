// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#pragma once

#include "ash/ash_export.h"

namespace aura_shell {

class AppListItemModel;

class ASH_EXPORT AppListViewDelegate {
 public:
  // AppListView owns the delegate.
  virtual ~AppListViewDelegate() {}

  // Invoked an AppListeItemModelView is  activated by click or enter key.
  virtual void OnAppListItemActivated(AppListItemModel* item,
                                      int event_flags) = 0;
};

}  // namespace aura_shell

#endif  // ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
