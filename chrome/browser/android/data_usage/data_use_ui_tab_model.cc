// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"

#include <utility>

#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Default maximum limit imposed for the initial UI navigation event buffer.
// Once this limit is reached, the buffer will be cleared.
const uint32_t kDefaultMaxNavigationEventsBuffered = 100;

}  // namespace

namespace chrome {

namespace android {

DataUseUITabModel::DataUseUITabModel()
    : data_use_ui_navigations_(new std::vector<DataUseUINavigationEvent>()),
      weak_factory_(this) {
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
    SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(0, tab_id);

  DataUseTabModel::TransitionType transition_type;

  if (!ConvertTransitionType(page_transition, gurl, &transition_type))
    return;

  if (data_use_ui_navigations_) {
    BufferNavigationEvent(tab_id, transition_type, gurl, std::string());
    return;
  }

  if (data_use_tab_model_) {
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
    const std::string& package_name,
    const std::string& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (tab_id <= 0)
    return;

  if (data_use_ui_navigations_) {
    BufferNavigationEvent(tab_id, DataUseTabModel::TRANSITION_CUSTOM_TAB,
                          GURL(url), package_name);
    return;
  }

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

  // Check if |data_use_tab_model_| is already ready for navigation events.
  if (data_use_tab_model_->is_ready_for_navigation_event())
    OnDataUseTabModelReady();
}

base::WeakPtr<DataUseUITabModel> DataUseUITabModel::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

void DataUseUITabModel::NotifyTrackingStarting(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not show tracking started UI for custom tabs with package match.
  if (data_use_tab_model_->IsCustomTabPackageMatch(tab_id))
    return;

  // Clear out the previous state if is equal to DATA_USE_CONTINUE_CLICKED. This
  // ensures that MaybeCreateTabEvent can successfully insert |tab_id| into the
  // map, and update its value to DATA_USE_TRACKING_STARTED.
  RemoveTabEvent(tab_id, DATA_USE_CONTINUE_CLICKED);

  if (MaybeCreateTabEvent(tab_id, DATA_USE_TRACKING_STARTED))
    return;
  // Since tracking started before the UI could indicate that it ended, it is
  // not useful for UI to show that it started again.
  RemoveTabEvent(tab_id, DATA_USE_TRACKING_ENDED);
}

void DataUseUITabModel::NotifyTrackingEnding(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!data_use_tab_model_->IsCustomTabPackageMatch(tab_id));

  if (MaybeCreateTabEvent(tab_id, DATA_USE_TRACKING_ENDED))
    return;
  // Since tracking ended before the UI could indicate that it stated, it is not
  // useful for UI to show that it ended.
  RemoveTabEvent(tab_id, DATA_USE_TRACKING_STARTED);

  // If the user clicked "Continue" before this navigation, then |tab_id| value
  // would be set to DATA_USE_CONTINUE_CLICKED. In that case,
  // MaybeCreateTabEvent above would have returned false. So, removing tab_id
  // from the map below ensures that no UI is shown on the tab. This also
  // resets the state of |tab_id|, so that dialog box or snackbar may be shown
  // for future navigations.
  RemoveTabEvent(tab_id, DATA_USE_CONTINUE_CLICKED);
}

bool DataUseUITabModel::CheckAndResetDataUseTrackingStarted(
    SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it == tab_events_.end())
    return false;

  return RemoveTabEvent(tab_id, DATA_USE_TRACKING_STARTED);
}

bool DataUseUITabModel::WouldDataUseTrackingEnd(
    const std::string& url,
    int page_transition,
    SessionID::id_type tab_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  TabEvents::const_iterator it = tab_events_.find(tab_id);

  if (it != tab_events_.end() && it->second == DATA_USE_CONTINUE_CLICKED)
    return false;

  DataUseTabModel::TransitionType transition_type;

  if (!ConvertTransitionType(ui::PageTransitionFromInt(page_transition),
                             GURL(url), &transition_type)) {
    return false;
  }

  if (!data_use_tab_model_)
    return false;

  return data_use_tab_model_->WouldNavigationEventEndTracking(
      tab_id, transition_type, GURL(url));
}

void DataUseUITabModel::UserClickedContinueOnDialogBox(
    SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  TabEvents::iterator it = tab_events_.find(tab_id);
  if (it != tab_events_.end())
    tab_events_.erase(it);

  MaybeCreateTabEvent(tab_id, DATA_USE_CONTINUE_CLICKED);
}

bool DataUseUITabModel::CheckAndResetDataUseTrackingEnded(
    SessionID::id_type tab_id) {
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
  if (it == tab_events_.end())
    return false;

  if (it->second == event) {
    tab_events_.erase(it);
    return true;
  }
  return false;
}

bool DataUseUITabModel::ConvertTransitionType(
    ui::PageTransition page_transition,
    const GURL& gurl,
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
      if (gurl == GURL(kChromeUIHistoryFrameURL) ||
          gurl == GURL(kChromeUIHistoryURL)) {
        // History menu.
        *transition_type = DataUseTabModel::TRANSITION_HISTORY_ITEM;
        return true;
      }
      return false;
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

void DataUseUITabModel::OnDataUseTabModelReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_use_tab_model_ &&
         data_use_tab_model_->is_ready_for_navigation_event());
  ProcessBufferedNavigationEvents();
}

void DataUseUITabModel::BufferNavigationEvent(
    SessionID::id_type tab_id,
    DataUseTabModel::TransitionType transition,
    const GURL& url,
    const std::string& package) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!data_use_tab_model_ ||
         !data_use_tab_model_->is_ready_for_navigation_event());

  data_use_ui_navigations_->push_back(
      DataUseUINavigationEvent(tab_id, transition, url, package));

  if (data_use_ui_navigations_->size() >= kDefaultMaxNavigationEventsBuffered) {
    ProcessBufferedNavigationEvents();
    // TODO(rajendrant): Add histogram to track that the buffer was full before
    // |SetDataUseTabModel| or |OnDataUseTabModelReady| got called.
  }
}

void DataUseUITabModel::ProcessBufferedNavigationEvents() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!data_use_ui_navigations_)
    return;

  // Move the ownership of vector managed by |data_use_ui_navigations_| and
  // release it, so that navigation events will be processed immediately.
  const auto tmp_data_use_ui_navigations_ = std::move(data_use_ui_navigations_);
  for (const auto& ui_event : *tmp_data_use_ui_navigations_.get()) {
    if (data_use_tab_model_) {
      data_use_tab_model_->OnNavigationEvent(ui_event.tab_id,
                                             ui_event.transition_type,
                                             ui_event.url, ui_event.package);
    }
  }
}

}  // namespace android

}  // namespace chrome
