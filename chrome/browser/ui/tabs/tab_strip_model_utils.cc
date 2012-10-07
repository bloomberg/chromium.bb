// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_utils.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace chrome {

int IndexOfFirstBlockedTab(const TabStripModel* model) {
  DCHECK(model);
  for (int i = 0; i < model->count(); ++i) {
    if (model->IsTabBlocked(i))
      return i;
  }
  // No blocked tabs.
  return model->count();
}

}  // namespace chrome
