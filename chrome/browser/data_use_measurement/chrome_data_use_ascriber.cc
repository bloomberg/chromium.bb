// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_recorder.h"
#include "components/data_use_measurement/content/content_url_request_classifier.h"
#include "components/data_use_measurement/core/data_use_recorder.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "ipc/ipc_message.h"
#include "net/url_request/url_request.h"

namespace data_use_measurement {

// static
const void* ChromeDataUseAscriber::DataUseRecorderEntryAsUserData::
    kUserDataKey = static_cast<void*>(
        &ChromeDataUseAscriber::DataUseRecorderEntryAsUserData::kUserDataKey);

ChromeDataUseAscriber::DataUseRecorderEntryAsUserData::
    DataUseRecorderEntryAsUserData(DataUseRecorderEntry entry)
    : entry_(entry) {}

ChromeDataUseAscriber::DataUseRecorderEntryAsUserData::
    ~DataUseRecorderEntryAsUserData() {}

ChromeDataUseAscriber::ChromeDataUseAscriber() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

ChromeDataUseAscriber::~ChromeDataUseAscriber() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(subframe_to_mainframe_map_.empty());
  // DCHECK(pending_navigation_global_request_id_.empty());
  // |data_use_recorders_| can be non empty, when mainframe url requests are
  // created but no mainframe navigations take place.
  // TODO(rajendrant): Enable this check when fixed for unittests.
  // DCHECK(data_use_recorders_.empty());
}

ChromeDataUseRecorder* ChromeDataUseAscriber::GetOrCreateDataUseRecorder(
    net::URLRequest* request) {
  DataUseRecorderEntry entry = GetOrCreateDataUseRecorderEntry(request);
  return entry == data_use_recorders_.end() ? nullptr : &(*entry);
}

ChromeDataUseRecorder* ChromeDataUseAscriber::GetDataUseRecorder(
    const net::URLRequest& request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // If a DataUseRecorder has already been set as user data, then return that.
  auto* user_data = static_cast<DataUseRecorderEntryAsUserData*>(
      request.GetUserData(DataUseRecorderEntryAsUserData::kUserDataKey));
  return user_data ? &(*user_data->recorder_entry()) : nullptr;
}

ChromeDataUseAscriber::DataUseRecorderEntry
ChromeDataUseAscriber::GetOrCreateDataUseRecorderEntry(
    net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // If a DataUseRecorder has already been set as user data, then return that.
  auto* user_data = static_cast<DataUseRecorderEntryAsUserData*>(
      request->GetUserData(DataUseRecorderEntryAsUserData::kUserDataKey));
  if (user_data)
    return user_data->recorder_entry();

  // If request is associated with a ChromeService, create a new
  // DataUseRecorder for it. There is no reason to aggregate URLRequests
  // from ChromeServices into the same DataUseRecorder instance.
  DataUseUserData* service = static_cast<DataUseUserData*>(
      request->GetUserData(DataUseUserData::kUserDataKey));
  if (service) {
    DataUseRecorderEntry entry =
        CreateNewDataUseRecorder(request, DataUse::TrafficType::SERVICES);
    entry->data_use().set_description(
        DataUseUserData::GetServiceNameAsString(service->service_name()));
    return entry;
  }

  if (!request->url().SchemeIsHTTPOrHTTPS())
    return data_use_recorders_.end();

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!request_info ||
      request_info->GetGlobalRequestID() == content::GlobalRequestID()) {
    // Create a new DataUseRecorder for all non-content initiated requests.
    DataUseRecorderEntry entry =
        CreateNewDataUseRecorder(request, DataUse::TrafficType::UNKNOWN);
    DataUse& data_use = entry->data_use();
    data_use.set_url(request->url());
    return entry;
  }

  if (request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    DataUseRecorderEntry new_entry =
        CreateNewDataUseRecorder(request, DataUse::TrafficType::USER_TRAFFIC);
    new_entry->set_main_frame_request_id(request_info->GetGlobalRequestID());
    pending_navigation_data_use_map_.insert(
        std::make_pair(request_info->GetGlobalRequestID(), new_entry));
    return new_entry;
  }

  int render_process_id = -1;
  int render_frame_id = -1;
  bool has_valid_frame = content::ResourceRequestInfo::GetRenderFrameForRequest(
      request, &render_process_id, &render_frame_id);
  if (has_valid_frame &&
      render_frame_id != SpecialRoutingIDs::MSG_ROUTING_NONE) {
    DCHECK(content::IsBrowserSideNavigationEnabled() ||
           render_process_id >= 0 || render_frame_id >= 0);

    // Browser tests may not set up DataUseWebContentsObservers in which case
    // this class never sees navigation and frame events so DataUseRecorders
    // will never be destroyed. To avoid this, we ignore requests whose
    // render frames don't have a record. However, this can also be caused by
    // URLRequests racing the frame create events.
    // TODO(kundaji): Add UMA.
    RenderFrameHostID frame_key(render_process_id, render_frame_id);
    const auto main_frame_key_iter = subframe_to_mainframe_map_.find(frame_key);
    if (main_frame_key_iter == subframe_to_mainframe_map_.end()) {
      return data_use_recorders_.end();
    }
    const auto frame_iter =
        main_render_frame_data_use_map_.find(main_frame_key_iter->second);
    if (frame_iter == main_render_frame_data_use_map_.end()) {
      return data_use_recorders_.end();
    }

    AscribeRecorderWithRequest(request, frame_iter->second);
    return frame_iter->second;
  }

  // Create a new DataUseRecorder for all other requests.
  DataUseRecorderEntry entry = CreateNewDataUseRecorder(
      request,
      content::ResourceRequestInfo::OriginatedFromServiceWorker(request)
          ? DataUse::TrafficType::SERVICE_WORKER
          : DataUse::TrafficType::UNKNOWN);
  DataUse& data_use = entry->data_use();
  data_use.set_url(request->url());
  return entry;
}

void ChromeDataUseAscriber::OnUrlRequestCompleted(
    const net::URLRequest& request,
    bool started) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ChromeDataUseRecorder* recorder = GetDataUseRecorder(request);

  if (!recorder)
    return;

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  if (!request_info ||
      request_info->GetResourceType() != content::RESOURCE_TYPE_MAIN_FRAME) {
    return;
  }

  // If mainframe request was not successful, then NavigationHandle in
  // DidFinishMainFrameNavigation will not have GlobalRequestID. So we erase the
  // DataUseRecorderEntry here.
  if (!request.status().is_success())
    pending_navigation_data_use_map_.erase(recorder->main_frame_request_id());
}

void ChromeDataUseAscriber::OnUrlRequestDestroyed(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(rajendrant): GetDataUseRecorder is sufficient and
  // GetOrCreateDataUseRecorderEntry is not needed. The entry gets created in
  // DataUseAscriber::OnBeforeUrlRequest().
  const DataUseRecorderEntry entry = GetOrCreateDataUseRecorderEntry(request);

  if (entry == data_use_recorders_.end())
    return;

  for (auto& observer : observers_)
    observer.OnPageResourceLoad(*request, &entry->data_use());

  const auto frame_iter =
      main_render_frame_data_use_map_.find(entry->main_frame_id());

  // Check whether the frame is tracked in the main render frame map, and if it
  // is, check if |entry| is currently tracked by that frame.
  bool frame_is_tracked = frame_iter != main_render_frame_data_use_map_.end() &&
                          frame_iter->second == entry;

  // For non-main frame requests, the page load can only be tracked in the frame
  // map.
  bool page_load_is_tracked = frame_is_tracked;

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);

  // If the frame is not tracked, but this is a main frame request, it might be
  // the case that the navigation has not commit yet.
  if (!frame_is_tracked && request_info &&
      request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    page_load_is_tracked =
        pending_navigation_data_use_map_.find(entry->main_frame_request_id()) !=
        pending_navigation_data_use_map_.end();
  }

  DataUseAscriber::OnUrlRequestDestroyed(request);

  // If all requests are done for |entry| and no more requests can be attributed
  // to it, it is safe to delete.
  if (entry->IsDataUseComplete() && !page_load_is_tracked) {
    NotifyDataUseCompleted(entry);
    data_use_recorders_.erase(entry);
  }
}

void ChromeDataUseAscriber::RenderFrameCreated(int render_process_id,
                                               int render_frame_id,
                                               int main_render_process_id,
                                               int main_render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (main_render_process_id != -1 && main_render_frame_id != -1) {
    // Create an entry in |subframe_to_mainframe_map_| for this frame mapped to
    // it's parent frame.
    subframe_to_mainframe_map_.insert(std::make_pair(
        RenderFrameHostID(render_process_id, render_frame_id),
        RenderFrameHostID(main_render_process_id, main_render_frame_id)));
  } else {
    subframe_to_mainframe_map_.insert(
        std::make_pair(RenderFrameHostID(render_process_id, render_frame_id),
                       RenderFrameHostID(render_process_id, render_frame_id)));
    auto frame_iter = main_render_frame_data_use_map_.find(
        RenderFrameHostID(render_process_id, render_frame_id));
    DCHECK(frame_iter == main_render_frame_data_use_map_.end());
    DataUseRecorderEntry entry =
        CreateNewDataUseRecorder(nullptr, DataUse::TrafficType::USER_TRAFFIC);
    entry->set_main_frame_id(
        RenderFrameHostID(render_process_id, render_frame_id));
    main_render_frame_data_use_map_.insert(std::make_pair(
        RenderFrameHostID(render_process_id, render_frame_id), entry));
  }
}

void ChromeDataUseAscriber::RenderFrameDeleted(int render_process_id,
                                               int render_frame_id,
                                               int main_render_process_id,
                                               int main_render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  RenderFrameHostID key(render_process_id, render_frame_id);

  if (main_render_process_id == -1 && main_render_frame_id == -1) {
    auto frame_iter = main_render_frame_data_use_map_.find(key);

    if (main_render_frame_data_use_map_.end() != frame_iter) {
      DataUseRecorderEntry entry = frame_iter->second;
      if (entry->IsDataUseComplete()) {
        NotifyDataUseCompleted(entry);
        data_use_recorders_.erase(entry);
      }
      main_render_frame_data_use_map_.erase(frame_iter);
    }
  }
  subframe_to_mainframe_map_.erase(key);
  visible_main_render_frames_.erase(key);
  pending_navigation_global_request_id_.erase(key);
}

void ChromeDataUseAscriber::DidStartMainFrameNavigation(
    GURL gurl,
    int render_process_id,
    int render_frame_id,
    void* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeDataUseAscriber::ReadyToCommitMainFrameNavigation(
    content::GlobalRequestID global_request_id,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  pending_navigation_global_request_id_.insert(
      std::make_pair(RenderFrameHostID(render_process_id, render_frame_id),
                     global_request_id));
}

void ChromeDataUseAscriber::DidFinishNavigation(int render_process_id,
                                                int render_frame_id,
                                                GURL gurl,
                                                bool is_same_page_navigation,
                                                uint32_t page_transition) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  RenderFrameHostID mainframe(render_process_id, render_frame_id);

  // Find the global request id of the pending navigation.
  auto global_request_id =
      pending_navigation_global_request_id_.find(mainframe);

  // TODO(rajendrant): Analyze why global request ID was not found in
  // pending navigation map, in tests.
  if (global_request_id == pending_navigation_global_request_id_.end())
    return;

  // Find the DataUseRecorderEntry the frame is associated with
  auto frame_it = main_render_frame_data_use_map_.find(mainframe);

  // Find the pending navigation entry.
  auto navigation_iter =
      pending_navigation_data_use_map_.find(global_request_id->second);
  pending_navigation_global_request_id_.erase(global_request_id);

  // We might not find a navigation entry since the pending navigation may not
  // have caused any HTTP or HTTPS URLRequests to be made.
  if (navigation_iter == pending_navigation_data_use_map_.end()) {
    // No pending navigation entry to worry about. However, the old frame entry
    // must be removed from frame map, and possibly marked complete and deleted.
    if (frame_it != main_render_frame_data_use_map_.end()) {
      const DataUseRecorderEntry old_frame_entry = frame_it->second;
      DataUse::TrafficType old_traffic_type =
          old_frame_entry->data_use().traffic_type();
      old_frame_entry->set_page_transition(page_transition);
      main_render_frame_data_use_map_.erase(frame_it);
      NotifyPageLoadCommit(old_frame_entry);
      if (old_frame_entry->IsDataUseComplete()) {
        NotifyDataUseCompleted(old_frame_entry);
        main_render_frame_data_use_map_.erase(old_frame_entry->main_frame_id());
        data_use_recorders_.erase(old_frame_entry);
      }

      // Add a new recorder to the render frame map to replace the deleted one.
      DataUseRecorderEntry entry = data_use_recorders_.emplace(
          data_use_recorders_.end(), old_traffic_type);
      entry->set_main_frame_id(mainframe);
      main_render_frame_data_use_map_.insert(std::make_pair(mainframe, entry));
    }
    return;
  }

  const DataUseRecorderEntry entry = navigation_iter->second;
  pending_navigation_data_use_map_.erase(navigation_iter);
  entry->set_main_frame_id(mainframe);

  // If the frame has already been deleted then mark this navigation as having
  // completed its data use.
  if (frame_it == main_render_frame_data_use_map_.end()) {
    entry->set_page_transition(page_transition);
    NotifyPageLoadCommit(entry);
    if (entry->IsDataUseComplete()) {
      NotifyDataUseCompleted(entry);
      main_render_frame_data_use_map_.erase(entry->main_frame_id());
      data_use_recorders_.erase(entry);
    }
    return;
  }
  DataUseRecorderEntry old_frame_entry = frame_it->second;
  old_frame_entry->set_page_transition(page_transition);
  NotifyPageLoadCommit(old_frame_entry);

  if (is_same_page_navigation) {
    old_frame_entry->MergeFrom(&(*entry));

    for (auto* request : entry->pending_url_requests())
      AscribeRecorderWithRequest(request, old_frame_entry);

    entry->RemoveAllPendingURLRequests();
    data_use_recorders_.erase(entry);
  } else {
    if (old_frame_entry->IsDataUseComplete()) {
      NotifyDataUseCompleted(old_frame_entry);
      data_use_recorders_.erase(old_frame_entry);

      if (visible_main_render_frames_.find(mainframe) !=
          visible_main_render_frames_.end()) {
        entry->set_is_visible(true);
      }
    }

    DataUse& data_use = entry->data_use();

    DCHECK(!data_use.url().is_valid() || data_use.url() == gurl)
        << "is valid: " << data_use.url().is_valid()
        << "; data_use.url(): " << data_use.url().spec()
        << "; gurl: " << gurl.spec();
    if (!data_use.url().is_valid()) {
      data_use.set_url(gurl);
    }

    main_render_frame_data_use_map_.erase(frame_it);
    main_render_frame_data_use_map_.insert(std::make_pair(mainframe, entry));
  }
}

void ChromeDataUseAscriber::NotifyPageLoadCommit(DataUseRecorderEntry entry) {
  for (auto& observer : observers_)
    observer.OnPageLoadCommit(&entry->data_use());
}

void ChromeDataUseAscriber::NotifyDataUseCompleted(DataUseRecorderEntry entry) {
  for (auto& observer : observers_)
    observer.OnPageLoadComplete(&entry->data_use());
}

std::unique_ptr<URLRequestClassifier>
ChromeDataUseAscriber::CreateURLRequestClassifier() const {
  return base::MakeUnique<ContentURLRequestClassifier>();
}

ChromeDataUseAscriber::DataUseRecorderEntry
ChromeDataUseAscriber::CreateNewDataUseRecorder(
    net::URLRequest* request,
    DataUse::TrafficType traffic_type) {
  DataUseRecorderEntry entry =
      data_use_recorders_.emplace(data_use_recorders_.end(), traffic_type);
  if (request)
    AscribeRecorderWithRequest(request, entry);
  return entry;
}

void ChromeDataUseAscriber::AscribeRecorderWithRequest(
    net::URLRequest* request,
    DataUseRecorderEntry recorder) {
  recorder->AddPendingURLRequest(request);
  request->SetUserData(
      DataUseRecorderEntryAsUserData::kUserDataKey,
      base::MakeUnique<DataUseRecorderEntryAsUserData>(recorder));
}

void ChromeDataUseAscriber::WasShownOrHidden(int main_render_process_id,
                                             int main_render_frame_id,
                                             bool visible) {
  RenderFrameHostID main_render_frame_host_id(main_render_process_id,
                                              main_render_frame_id);

  auto frame_iter =
      main_render_frame_data_use_map_.find(main_render_frame_host_id);
  if (frame_iter != main_render_frame_data_use_map_.end())
    frame_iter->second->set_is_visible(visible);

  if (visible) {
    visible_main_render_frames_.insert(main_render_frame_host_id);
  } else {
    visible_main_render_frames_.erase(main_render_frame_host_id);
  }
}

void ChromeDataUseAscriber::RenderFrameHostChanged(int old_render_process_id,
                                                   int old_render_frame_id,
                                                   int new_render_process_id,
                                                   int new_render_frame_id) {
  RenderFrameHostID old_frame(old_render_process_id, old_render_frame_id);
  if (visible_main_render_frames_.find(old_frame) !=
      visible_main_render_frames_.end()) {
    WasShownOrHidden(new_render_process_id, new_render_frame_id, true);
  }
  auto pending_navigation_iter =
      pending_navigation_global_request_id_.find(old_frame);
  if (pending_navigation_iter != pending_navigation_global_request_id_.end()) {
    pending_navigation_global_request_id_.insert(std::make_pair(
        RenderFrameHostID(new_render_process_id, new_render_frame_id),
        pending_navigation_iter->second));
    pending_navigation_global_request_id_.erase(pending_navigation_iter);
  }
}

}  // namespace data_use_measurement
