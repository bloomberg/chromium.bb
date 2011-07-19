// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_SYNC_GLUE_DO_OPTIMISTIC_REFRESH_TASK_H_
#define CHROME_BROWSER_SYNC_GLUE_DO_OPTIMISTIC_REFRESH_TASK_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "chrome/browser/autofill/personal_data_manager.h"

namespace browser_sync {

// A task used by this class and the change processor to inform the
// PersonalDataManager living on the UI thread that it needs to refresh.
class DoOptimisticRefreshForAutofill : public Task {
 public:
  explicit DoOptimisticRefreshForAutofill(PersonalDataManager* pdm);
  virtual ~DoOptimisticRefreshForAutofill();
  virtual void Run();
 private:
  scoped_refptr<PersonalDataManager> pdm_;
};

}  // namespace browser_sync
#endif  // CHROME_BROWSER_SYNC_GLUE_DO_OPTIMISTIC_REFRESH_TASK_H_

