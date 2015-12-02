// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"

#include <utility>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

DataUseUITabModel::DataUseUITabModel(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner), weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(io_task_runner_);
}

DataUseUITabModel::~DataUseUITabModel() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DataUseUITabModel::ReportBrowserNavigation(
    const GURL& gurl,
    ui::PageTransition page_transition,
    SessionID::id_type tab_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(0, tab_id);

  DataUseTabModel::TransitionType transition_type;

  if (ConvertTransitionType(page_transition, &transition_type)) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DataUseTabModel::OnNavigationEvent, data_use_tab_model_,
                   tab_id, transition_type, gurl, std::string()));
  }
}

void DataUseUITabModel::ReportTabClosure(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(0, tab_id);

  io_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&DataUseTabModel::OnTabCloseEvent,
                                       data_use_tab_model_, tab_id));

  // Clear out local state.
  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it == tab_events_.end())
    return;
  tab_events_.erase(it);
}

void DataUseUITabModel::ReportCustomTabInitialNavigation(
    SessionID::id_type tab_id,
    const std::string& url,
    const std::string& package_name) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (tab_id <= 0)
    return;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataUseTabModel::OnNavigationEvent, data_use_tab_model_,
                 tab_id, DataUseTabModel::TRANSITION_FROM_EXTERNAL_APP,
                 GURL(url), package_name));
}

void DataUseUITabModel::SetDataUseTabModel(
    base::WeakPtr<DataUseTabModel> data_use_tab_model) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_use_tab_model_ = data_use_tab_model;
}

base::WeakPtr<DataUseUITabModel> DataUseUITabModel::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

void DataUseUITabModel::NotifyTrackingStarting(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (MaybeCreateTabEvent(tab_id, DATA_USE_TRACKING_STARTED))
    return;
  // Since tracking started before the UI could indicate that it ended, it is
  // not useful for UI to show that it started again.
  RemoveTabEvent(tab_id, DATA_USE_TRACKING_ENDED);
}

void DataUseUITabModel::NotifyTrackingEnding(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (MaybeCreateTabEvent(tab_id, DATA_USE_TRACKING_ENDED))
    return;
  // Since tracking ended before the UI could indicate that it stated, it is not
  // useful for UI to show that it ended.
  RemoveTabEvent(tab_id, DATA_USE_TRACKING_STARTED);
}

bool DataUseUITabModel::HasDataUseTrackingStarted(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it == tab_events_.end())
    return false;

  return RemoveTabEvent(tab_id, DATA_USE_TRACKING_STARTED);
}

bool DataUseUITabModel::HasDataUseTrackingEnded(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it == tab_events_.end())
    return false;

  return RemoveTabEvent(tab_id, DATA_USE_TRACKING_ENDED);
}

bool DataUseUITabModel::MaybeCreateTabEvent(SessionID::id_type tab_id,
                                            DataUseTrackingEvent event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return tab_events_.insert(std::make_pair(tab_id, event)).second;
}

bool DataUseUITabModel::RemoveTabEvent(SessionID::id_type tab_id,
                                       DataUseTrackingEvent event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TabEvents::iterator it = tab_events_.find(tab_id);
  DCHECK(it != tab_events_.end());
  if (it->second == event) {
    tab_events_.erase(it);
    return true;
  }
  return false;
}

bool DataUseUITabModel::ConvertTransitionType(
    ui::PageTransition page_transition,
    DataUseTabModel::TransitionType* transition_type) const {
  if (!ui::PageTransitionIsMainFrame(page_transition) ||
      !ui::PageTransitionIsNewNavigation(page_transition)) {
    return false;
  }

  switch (page_transition & ui::PAGE_TRANSITION_CORE_MASK) {
    case ui::PAGE_TRANSITION_LINK:
      if ((page_transition & ui::PAGE_TRANSITION_FROM_API) != 0) {
        // Clicking on bookmarks.
        *transition_type = DataUseTabModel::TRANSITION_BOOKMARK;
        return true;
      }
      return false;  // Newtab, clicking on a link.
    case ui::PAGE_TRANSITION_TYPED:
      *transition_type = DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION;
      return true;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      // Auto bookmark from newtab page.
      *transition_type = DataUseTabModel::TRANSITION_BOOKMARK;
      return true;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      // History menu.
      *transition_type = DataUseTabModel::TRANSITION_HISTORY_ITEM;
      return true;
    case ui::PAGE_TRANSITION_GENERATED:
      // Omnibox search (e.g., searching for "tacos").
      *transition_type = DataUseTabModel::TRANSITION_OMNIBOX_SEARCH;
      return true;
    case ui::PAGE_TRANSITION_RELOAD:
      // Restored tabs.
      return false;
    default:
      return false;
  }
}

}  // namespace android

}  // namespace chrome
