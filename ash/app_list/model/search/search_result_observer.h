// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_OBSERVER_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_OBSERVER_H_

#include "ash/app_list/model/app_list_model_export.h"

namespace ash {

class APP_LIST_MODEL_EXPORT SearchResultObserver {
 public:
  // Invoked when the SearchResult's metadata has changed.
  virtual void OnMetadataChanged() {}

  // Invoked just before the SearchResult is destroyed.
  virtual void OnResultDestroying() {}

 protected:
  virtual ~SearchResultObserver() {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_OBSERVER_H_
