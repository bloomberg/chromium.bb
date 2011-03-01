// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/do_optimistic_refresh_task.h"

#include "chrome/browser/autofill/personal_data_manager.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

DoOptimisticRefreshForAutofill::DoOptimisticRefreshForAutofill(
  PersonalDataManager* pdm) : pdm_(pdm) {}

DoOptimisticRefreshForAutofill::~DoOptimisticRefreshForAutofill() {}

void DoOptimisticRefreshForAutofill::Run() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pdm_->Refresh();
}

}  // namespace browser_sync

