// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_PAGINATION_MODEL_OBSERVER_H_
#define ASH_APP_LIST_PAGINATION_MODEL_OBSERVER_H_
#pragma once

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT PaginationModelObserver {
 public:
  // Invoked when the total number of page is changed.
  virtual void TotalPagesChanged() = 0;

  // Invoked when the selected page index is changed.
  virtual void SelectedPageChanged(int old_selected, int new_selected) = 0;

 protected:
  virtual ~PaginationModelObserver() {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_PAGINATION_MODEL_OBSERVER_H_
