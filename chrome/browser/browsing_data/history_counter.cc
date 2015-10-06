// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/history_counter.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"

using history::HistoryService;
using history::HistoryCountResult;

HistoryCounter::HistoryCounter() : pref_name_(prefs::kDeleteBrowsingHistory) {}

HistoryCounter::~HistoryCounter() {
}

const std::string& HistoryCounter::GetPrefName() const {
  return pref_name_;
}

bool HistoryCounter::HasTrackedTasks() {
  return cancelable_task_tracker_.HasTrackedTasks();
}

void HistoryCounter::Count() {
  cancelable_task_tracker_.TryCancelAll();

  HistoryService* service =
      HistoryServiceFactory::GetForProfileWithoutCreating(GetProfile());

  service->GetHistoryCount(
      GetPeriodStart(),
      base::Time::Max(),
      base::Bind(
          &HistoryCounter::OnGetLocalHistoryCount, base::Unretained(this)),
      &cancelable_task_tracker_);

  // TODO(msramek): Include web history results.
}

void HistoryCounter::OnGetLocalHistoryCount(HistoryCountResult result) {
  if (!result.success) {
    LOG(ERROR) << "Failed to count the local history.";
    return;
  }

  ReportResult(result.count);
}
