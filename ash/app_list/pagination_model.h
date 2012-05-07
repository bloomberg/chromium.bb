// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_PAGINATION_MODEL_H_
#define ASH_APP_LIST_PAGINATION_MODEL_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/observer_list.h"

namespace ash {

class PaginationModelObserver;

// A simple pagination model that consists of two numbers: the total pages and
// the currently selected page. The model is a single selection model that at
// the most one page can become selected at any time.
class ASH_EXPORT PaginationModel {
 public:
  PaginationModel();
  ~PaginationModel();

  void SetTotalPages(int total_pages);
  void SelectPage(int page);

  void AddObserver(PaginationModelObserver* observer);
  void RemoveObserver(PaginationModelObserver* observer);

  int total_pages() const {
    return total_pages_;
  }

  int selected_page() const {
    return selected_page_;
  }

 private:
  int total_pages_;
  int selected_page_;
  ObserverList<PaginationModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(PaginationModel);
};

}  // namespace ash

#endif  // ASH_APP_LIST_PAGINATION_MODEL_H_
