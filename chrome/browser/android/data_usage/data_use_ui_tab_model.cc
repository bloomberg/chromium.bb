// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"

#include <utility>

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

DataUseUITabModel::DataUseUITabModel() : weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

DataUseUITabModel::~DataUseUITabModel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (data_use_tab_model_)
    data_use_tab_model_->RemoveObserver(this);
}

void DataUseUITabModel::ReportBrowserNavigation(
    const GURL& gurl,
    ui::PageTransition page_transition,
    SessionID::id_type tab_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(0, tab_id);

  DataUseTabModel::TransitionType transition_type;

  if (data_use_tab_model_ &&
      ConvertTransitionType(page_transition, &transition_type)) {
    data_use_tab_model_->OnNavigationEvent(tab_id, transition_type, gurl,
                                           std::string());
  }
}

void DataUseUITabModel::ReportTabClosure(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(0, tab_id);
  if (data_use_tab_model_)
    data_use_tab_model_->OnTabCloseEvent(tab_id);

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

  if (data_use_tab_model_) {
    data_use_tab_model_->OnNavigationEvent(
        tab_id, DataUseTabModel::TRANSITION_CUSTOM_TAB, GURL(url),
        package_name);
  }
}

void DataUseUITabModel::SetDataUseTabModel(
    DataUseTabModel* data_use_tab_model) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!data_use_tab_model)
    return;

  data_use_tab_model_ = data_use_tab_model->GetWeakPtr();
  data_use_tab_model_->AddObserver(this);
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
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!ui::PageTransitionIsMainFrame(page_transition) ||
      (((page_transition & ui::PAGE_TRANSITION_CORE_MASK) !=
        ui::PAGE_TRANSITION_RELOAD) &&
       !ui::PageTransitionIsNewNavigation(page_transition))) {
    return false;
  }

  switch (page_transition & ui::PAGE_TRANSITION_CORE_MASK) {
    case ui::PAGE_TRANSITION_LINK:
      if ((page_transition & ui::PAGE_TRANSITION_FROM_API) != 0) {
        // Clicking on bookmarks.
        *transition_type = DataUseTabModel::TRANSITION_BOOKMARK;
        return true;
      }
      // Newtab, clicking on a link.
      *transition_type = DataUseTabModel::TRANSITION_LINK;
      return true;
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
      if ((page_transition & ui::PAGE_TRANSITION_FORWARD_BACK) == 0) {
        // Restored tabs or user reloaded the page.
        // TODO(rajendrant): Handle only the tab restore case. We are only
        // interested in that.
        *transition_type = DataUseTabModel::TRANSITION_RELOAD;
        return true;
      }
      return false;
    default:
      return false;
  }
}

}  // namespace android

}  // namespace chrome
