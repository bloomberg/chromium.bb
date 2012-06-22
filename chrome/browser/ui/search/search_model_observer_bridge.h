// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_OBSERVER_BRIDGE_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_OBSERVER_BRIDGE_H_
#pragma once

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_model_observer.h"

namespace chrome {
namespace search {

template <class Receiver>
class SearchModelObserverBridge : public SearchModelObserver {
 public:
  SearchModelObserverBridge(Receiver* receiver, SearchModel* model)
      : receiver_(receiver),
        model_(model) {
    model_->AddObserver(this);
  }

  ~SearchModelObserverBridge() {
    model_->RemoveObserver(this);
  }

  // SearchModelObserver:
  virtual void ModeChanged(const Mode& mode) OVERRIDE {
    [receiver_ modeChanged:mode];
  }

 private:
  // Weak.  Owns us.
  Receiver* receiver_;

  // Weak.
  SearchModel* model_;

  DISALLOW_COPY_AND_ASSIGN(SearchModelObserverBridge);
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_OBSERVER_BRIDGE_H_
