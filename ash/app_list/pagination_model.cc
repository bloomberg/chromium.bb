// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/pagination_model.h"

#include "ash/app_list/pagination_model_observer.h"

namespace ash {

PaginationModel::PaginationModel()
    : total_pages_(-1),
      selected_page_(-1) {
}

PaginationModel::~PaginationModel() {
}

void PaginationModel::SetTotalPages(int total_pages) {
  if (total_pages == total_pages_)
    return;

  total_pages_ = total_pages;
  FOR_EACH_OBSERVER(PaginationModelObserver,
                    observers_,
                    TotalPagesChanged());
}

void PaginationModel::SelectPage(int page) {
  DCHECK(page >= 0 && page < total_pages_);

  if (page == selected_page_)
    return;

  int old_selected = selected_page_;
  selected_page_ = page;
  FOR_EACH_OBSERVER(PaginationModelObserver,
                    observers_,
                    SelectedPageChanged(old_selected, selected_page_));
}

void PaginationModel::AddObserver(PaginationModelObserver* observer) {
  observers_.AddObserver(observer);
}

void PaginationModel::RemoveObserver(PaginationModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
