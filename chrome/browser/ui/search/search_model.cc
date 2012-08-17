// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"

namespace chrome {
namespace search {

SearchModel::SearchModel(TabContents* contents)
    : contents_(contents) {
}

SearchModel::~SearchModel() {
}

void SearchModel::SetMode(const Mode& new_mode) {
  if (!contents_)
    return;

  DCHECK(IsInstantExtendedAPIEnabled(contents_->profile()))
      << "Please do not try to set the SearchModel mode without first "
      << "checking if Search is enabled.";

  if (mode_ == new_mode)
    return;

  const Mode old_mode = mode_;
  mode_ = new_mode;

  FOR_EACH_OBSERVER(SearchModelObserver, observers_,
                    ModeChanged(old_mode, mode_));

  // Animation is transient, it is cleared after observers are notified.
  mode_.animate = false;
}

void SearchModel::AddObserver(SearchModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchModel::RemoveObserver(SearchModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

} //  namespace search
} //  namespace chrome
