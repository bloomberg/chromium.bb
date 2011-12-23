// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_ITEM_MODEL_OBSERVER_H_
#define ASH_APP_LIST_APP_LIST_ITEM_MODEL_OBSERVER_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace aura_shell {

class AURA_SHELL_EXPORT AppListItemModelObserver {
 public:
  // Invoked after app list item's icon is changed.
  virtual void ItemIconChanged() = 0;

  // Invoked after app list item's title is changed.
  virtual void ItemTitleChanged() = 0;

 protected:
  virtual ~AppListItemModelObserver() {}
};

}  // namespace aura_shell

#endif  // ASH_APP_LIST_APP_LIST_ITEM_MODEL_OBSERVER_H_
