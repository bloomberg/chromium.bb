// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

DataUseUITabModel::DataUseUITabModel(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner) {
  DCHECK(io_task_runner_);
}

DataUseUITabModel::~DataUseUITabModel() {}

void DataUseUITabModel::ReportBrowserNavigation(
    const GURL& gurl,
    ui::PageTransition page_transition,
    int32_t tab_id) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(tbansal): Post to DataUseTabModel on IO thread.
}

void DataUseUITabModel::ReportTabClosure(int32_t tab_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(tbansal): Post to DataUseTabModel on IO thread.

  // Clear out local state.
  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it == tab_events_.end())
    return;
  tab_events_.erase(it);
}

void DataUseUITabModel::ReportCustomTabInitialNavigation(
    int32_t tab_id,
    const std::string& url,
    const std::string& package_name) {
  // TODO(tbansal): Post to DataUseTabModel on IO thread.
}

void DataUseUITabModel::OnTrackingStarted(int32_t tab_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(thread_checker_.CalledOnValidThread());

  if (MaybeCreateTabEvent(tab_id, DATA_USE_TRACKING_STARTED))
    return;
  // Since tracking started before the UI could indicate that it ended, it is
  // not useful for UI to show that it started again.
  RemoveTabEvent(tab_id, DATA_USE_TRACKING_ENDED);
}

void DataUseUITabModel::OnTrackingEnded(int32_t tab_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(thread_checker_.CalledOnValidThread());

  if (MaybeCreateTabEvent(tab_id, DATA_USE_TRACKING_ENDED))
    return;
  // Since tracking ended before the UI could indicate that it stated, it is not
  // useful for UI to show that it ended.
  RemoveTabEvent(tab_id, DATA_USE_TRACKING_STARTED);
}

bool DataUseUITabModel::HasDataUseTrackingStarted(int32_t tab_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(thread_checker_.CalledOnValidThread());

  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it == tab_events_.end())
    return false;

  return RemoveTabEvent(tab_id, DATA_USE_TRACKING_STARTED);
}

bool DataUseUITabModel::HasDataUseTrackingEnded(int32_t tab_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(thread_checker_.CalledOnValidThread());

  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it == tab_events_.end())
    return false;

  return RemoveTabEvent(tab_id, DATA_USE_TRACKING_ENDED);
}

bool DataUseUITabModel::MaybeCreateTabEvent(int32_t tab_id,
                                            DataUseTrackingEvent event) {
  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it == tab_events_.end()) {
    tab_events_.insert(std::make_pair(tab_id, event));
    return true;
  }
  return false;
}

bool DataUseUITabModel::RemoveTabEvent(int32_t tab_id,
                                       DataUseTrackingEvent event) {
  TabEvents::iterator it = tab_events_.find(tab_id);
  DCHECK(it != tab_events_.end());
  if (it->second == event) {
    tab_events_.erase(it);
    return true;
  }
  return false;
}

}  // namespace android

}  // namespace chrome
