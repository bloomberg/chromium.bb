// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "chrome/browser/ui/search/search_model_observer.h"
#include "components/search/search.h"

SearchModel::SearchModel() = default;

SearchModel::~SearchModel() = default;

void SearchModel::SetMode(const SearchMode& new_mode) {
  DCHECK(search::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel mode without first "
      << "checking if Search is enabled.";

  if (mode_ == new_mode)
    return;

  const SearchMode old_mode = mode_;
  mode_ = new_mode;

  for (SearchModelObserver& observer : observers_)
    observer.ModelChanged(old_mode, mode_);
}

void SearchModel::AddObserver(SearchModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchModel::RemoveObserver(SearchModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
