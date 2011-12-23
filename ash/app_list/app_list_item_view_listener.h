// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_ITEM_VIEW_LISTENER_H_
#define ASH_APP_LIST_APP_LIST_ITEM_VIEW_LISTENER_H_
#pragma once

#include "ash/ash_export.h"

namespace aura_shell {

class AppListItemView;

class ASH_EXPORT AppListItemViewListener {
 public:
  // Invoked when an AppListeItemModelView is activated by click or enter key.
  virtual void AppListItemActivated(AppListItemView* sender,
                                    int event_flags) = 0;

 protected:
  virtual ~AppListItemViewListener() {}
};

}  // namespace aura_shell

#endif  // ASH_APP_LIST_APP_LIST_ITEM_VIEW_LISTENER_H_
