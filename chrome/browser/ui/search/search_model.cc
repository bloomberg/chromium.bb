// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "chrome/browser/ui/search/search_model_observer.h"
#include "components/search/search.h"

SearchModel::SearchModel() : origin_(Origin::DEFAULT) {}

SearchModel::~SearchModel() = default;

void SearchModel::SetOrigin(Origin origin) {
  DCHECK(search::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel mode without first "
      << "checking if Search is enabled.";

  if (origin_ == origin)
    return;

  const Origin old_origin = origin_;
  origin_ = origin;

  for (SearchModelObserver& observer : observers_)
    observer.ModelChanged(old_origin, origin_);
}

void SearchModel::AddObserver(SearchModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchModel::RemoveObserver(SearchModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
