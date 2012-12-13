// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "content/public/browser/web_contents.h"

namespace chrome {
namespace search {

SearchModel::SearchModel(content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

SearchModel::~SearchModel() {
}

void SearchModel::SetMode(const Mode& new_mode) {
  if (!web_contents_)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  DCHECK(IsInstantExtendedAPIEnabled(profile))
      << "Please do not try to set the SearchModel mode without first "
      << "checking if Search is enabled.";

  if (mode_ == new_mode)
    return;

  const Mode old_mode = mode_;
  mode_ = new_mode;

  FOR_EACH_OBSERVER(SearchModelObserver, observers_,
                    ModeChanged(old_mode, mode_));
}

void SearchModel::AddObserver(SearchModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchModel::RemoveObserver(SearchModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

} //  namespace search
} //  namespace chrome
