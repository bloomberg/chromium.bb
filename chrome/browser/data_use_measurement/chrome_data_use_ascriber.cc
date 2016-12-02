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
  DCHECK_EQ(0u, data_use_recorders_.size());
}

ChromeDataUseRecorder* ChromeDataUseAscriber::GetDataUseRecorder(
    net::URLRequest* request,
    bool can_create_new) {
  DataUseRecorderEntry entry = GetDataUseRecorderEntry(request, can_create_new);
  return entry == data_use_recorders_.end() ? nullptr : &(*entry);
}

ChromeDataUseAscriber::DataUseRecorderEntry
ChromeDataUseAscriber::GetDataUseRecorderEntry(net::URLRequest* request,
                                               bool can_create_new) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(ryansturm): Handle PlzNavigate (http://crbug/664233).
  if (content::IsBrowserSideNavigationEnabled())
    return data_use_recorders_.end();

  // If a DataUseRecorder has already been set as user data, then return that.
  auto user_data = static_cast<DataUseRecorderEntryAsUserData*>(
      request->GetUserData(DataUseRecorderEntryAsUserData::kUserDataKey));
  if (user_data)
    return user_data->recorder_entry();

  if (!can_create_new)
    return data_use_recorders_.end();

  // If request is associated with a ChromeService, create a new
  // DataUseRecorder for it. There is no reason to aggregate URLRequests
  // from ChromeServices into the same DataUseRecorder instance.
  DataUseUserData* service = static_cast<DataUseUserData*>(
      request->GetUserData(DataUseUserData::kUserDataKey));
  if (service) {
    DataUseRecorderEntry entry = CreateNewDataUseRecorder(request);

    entry->data_use().set_description(
        DataUseUserData::GetServiceNameAsString(service->service_name()));
    return entry;
  }

  int render_process_id = -1;
  int render_frame_id = -1;
  bool has_valid_frame = content::ResourceRequestInfo::GetRenderFrameForRequest(
      request, &render_process_id, &render_frame_id);
  if (has_valid_frame &&
      render_frame_id != SpecialRoutingIDs::MSG_ROUTING_NONE) {
    DCHECK(render_process_id >= 0 || render_frame_id >= 0);

    // Browser tests may not set up DataUseWebContentsObservers in which case
    // this class never sees navigation and frame events so DataUseRecorders
    // will never be destroyed. To avoid this, we ignore requests whose
    // render frames don't have a record. However, this can also be caused by
    // URLRequests racing the frame create events.
    // TODO(kundaji): Add UMA.
    RenderFrameHostID frame_key(render_process_id, render_frame_id);
    auto frame_iter = render_frame_data_use_map_.find(frame_key);
    if (frame_iter == render_frame_data_use_map_.end()) {
      return data_use_recorders_.end();
    }

    const content::ResourceRequestInfo* request_info =
        content::ResourceRequestInfo::ForRequest(request);
    content::ResourceType resource_type =
        request_info ? request_info->GetResourceType()
                     : content::RESOURCE_TYPE_LAST_TYPE;

    if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
      content::GlobalRequestID navigation_key =
          request_info->GetGlobalRequestID();

      DataUseRecorderEntry new_entry = CreateNewDataUseRecorder(request);
      new_entry->set_main_frame_request_id(navigation_key);
      pending_navigation_data_use_map_.insert(
          std::make_pair(navigation_key, new_entry));

      return new_entry;
    }

    DCHECK(frame_iter != render_frame_data_use_map_.end());
    auto entry = frame_iter->second;
    request->SetUserData(DataUseRecorderEntryAsUserData::kUserDataKey,
                         new DataUseRecorderEntryAsUserData(entry));
    entry->AddPendingURLRequest(request);
    return entry;
  }

  // Create a new DataUseRecorder for all other requests.
  DataUseRecorderEntry entry = CreateNewDataUseRecorder(request);
  DataUse& data_use = entry->data_use();
  data_use.set_url(request->url());
  return entry;
}

void ChromeDataUseAscriber::OnBeforeUrlRequest(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DataUseAscriber::OnBeforeUrlRequest(request);

  // TODO(kundaji): Handle PlzNavigate.
  if (content::IsBrowserSideNavigationEnabled())
    return;

  auto service = static_cast<DataUseUserData*>(
      request->GetUserData(DataUseUserData::kUserDataKey));
  if (service)
    return;

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  content::ResourceType resource_type = request_info
                                            ? request_info->GetResourceType()
                                            : content::RESOURCE_TYPE_LAST_TYPE;

  if (resource_type != content::RESOURCE_TYPE_MAIN_FRAME)
    return;

  int render_process_id = -1;
  int render_frame_id = -1;
  bool has_valid_render_frame_id =
      content::ResourceRequestInfo::GetRenderFrameForRequest(
          request, &render_process_id, &render_frame_id);
  DCHECK(has_valid_render_frame_id);
  // Browser tests may not set up DataUseWebContentsObservers in which case
  // this class never sees navigation and frame events so DataUseRecorders
  // will never be destroyed. To avoid this, we ignore requests whose
  // render frames don't have a record. However, this can also be caused by
  // URLRequests racing the frame create events.
  // TODO(kundaji): Add UMA.
  if (render_frame_data_use_map_.find(
          RenderFrameHostID(render_process_id, render_frame_id)) ==
      render_frame_data_use_map_.end()) {
    return;
  }

  // If this request is already being tracked, do not create a new entry.
  auto user_data = static_cast<DataUseRecorderEntryAsUserData*>(
      request->GetUserData(DataUseRecorderEntryAsUserData::kUserDataKey));
  if (user_data)
    return;

  DataUseRecorderEntry entry = data_use_recorders_.emplace(
      data_use_recorders_.end());
  request->SetUserData(DataUseRecorderEntryAsUserData::kUserDataKey,
                       new DataUseRecorderEntryAsUserData(entry));
  pending_navigation_data_use_map_.insert(
      std::make_pair(request_info->GetGlobalRequestID(), entry));
}

void ChromeDataUseAscriber::OnUrlRequestDestroyed(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DataUseRecorderEntry entry = GetDataUseRecorderEntry(request, true);

  if (entry == data_use_recorders_.end())
    return;

  DataUseRecorder* recorder = &(*entry);

  RenderFrameHostID frame_key = entry->main_frame_id();
  auto frame_iter = render_frame_data_use_map_.find(frame_key);
  bool is_in_render_frame_map =
      frame_iter != render_frame_data_use_map_.end() &&
      frame_iter->second->HasPendingURLRequest(request);

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  content::ResourceType resource_type = request_info
                                            ? request_info->GetResourceType()
                                            : content::RESOURCE_TYPE_LAST_TYPE;

  bool is_in_pending_navigation_map = false;
  if (request_info && resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    auto navigation_iter = pending_navigation_data_use_map_.find(
        entry->main_frame_request_id());
    is_in_pending_navigation_map =
        navigation_iter != pending_navigation_data_use_map_.end();

    // If request was not successful, then NavigationHandle in
    // DidFinishMainFrameNavigation will not have GlobalRequestID. So we erase
    // the DataUseRecorderEntry here.
    if (is_in_pending_navigation_map && !request->status().is_success()) {
      pending_navigation_data_use_map_.erase(navigation_iter);
      is_in_pending_navigation_map = false;
    }
  }

  DataUseAscriber::OnUrlRequestDestroyed(request);
  request->RemoveUserData(DataUseRecorderEntryAsUserData::kUserDataKey);

  if (recorder->IsDataUseComplete() && !is_in_render_frame_map &&
      !is_in_pending_navigation_map) {
    OnDataUseCompleted(entry);
    data_use_recorders_.erase(entry);
  }
}

void ChromeDataUseAscriber::RenderFrameCreated(int render_process_id,
                                               int render_frame_id,
                                               int parent_render_process_id,
                                               int parent_render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (content::IsBrowserSideNavigationEnabled())
    return;

  if (parent_render_process_id != -1 && parent_render_frame_id != -1) {
    // Create an entry in |render_frame_data_use_map_| for this frame with
    // the same DataUseRecorderEntry as its parent frame.
    auto frame_iter = render_frame_data_use_map_.find(
        RenderFrameHostID(parent_render_process_id, parent_render_frame_id));

    DCHECK(frame_iter != render_frame_data_use_map_.end());

    DataUseRecorderEntry entry = frame_iter->second;
    render_frame_data_use_map_.insert(std::make_pair(
        RenderFrameHostID(render_process_id, render_frame_id), entry));
  } else {
    auto frame_iter = render_frame_data_use_map_.find(
        RenderFrameHostID(render_process_id, render_frame_id));
    DCHECK(frame_iter == render_frame_data_use_map_.end());
    DataUseRecorderEntry entry = CreateNewDataUseRecorder(nullptr);
    entry->set_main_frame_id(
        RenderFrameHostID(render_process_id, render_frame_id));
    render_frame_data_use_map_.insert(std::make_pair(
        RenderFrameHostID(render_process_id, render_frame_id), entry));
  }
}

void ChromeDataUseAscriber::RenderFrameDeleted(int render_process_id,
                                               int render_frame_id,
                                               int parent_render_process_id,
                                               int parent_render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (content::IsBrowserSideNavigationEnabled())
    return;

  RenderFrameHostID key(render_process_id, render_frame_id);
  auto frame_iter = render_frame_data_use_map_.find(key);

  DCHECK(frame_iter != render_frame_data_use_map_.end());

  DataUseRecorderEntry entry = frame_iter->second;
  DataUseRecorder* recorder = &(*entry);

  if (parent_render_process_id == -1 && parent_render_frame_id == -1 &&
      recorder->IsDataUseComplete()) {
    OnDataUseCompleted(entry);
    data_use_recorders_.erase(entry);
  }

  render_frame_data_use_map_.erase(frame_iter);
}

void ChromeDataUseAscriber::DidStartMainFrameNavigation(
    GURL gurl,
    int render_process_id,
    int render_frame_id,
    void* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeDataUseAscriber::ReadyToCommitMainFrameNavigation(
    GURL gurl,
    content::GlobalRequestID global_request_id,
    int render_process_id,
    int render_frame_id,
    bool is_same_page_navigation,
    void* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Find the DataUseRecorderEntry the frame is associated with
  auto frame_it = render_frame_data_use_map_.find(
      RenderFrameHostID(render_process_id, render_frame_id));

  // Find the pending navigation entry.
  auto navigation_iter =
      pending_navigation_data_use_map_.find(global_request_id);
  // We might not find a navigation entry since the pending navigation may not
  // have caused any HTTP or HTTPS URLRequests to be made.
  if (navigation_iter == pending_navigation_data_use_map_.end()) {
    // No pending navigation entry to worry about. However, the old frame entry
    // must be removed from frame map, and possibly marked complete and deleted.
    if (frame_it != render_frame_data_use_map_.end()) {
      DataUseRecorderEntry old_frame_entry = frame_it->second;
      render_frame_data_use_map_.erase(frame_it);
      if (old_frame_entry->IsDataUseComplete()) {
        OnDataUseCompleted(old_frame_entry);
        data_use_recorders_.erase(old_frame_entry);
      }

      // Add a new recorder to the render frame map to replace the deleted one.
      DataUseRecorderEntry entry = data_use_recorders_.emplace(
          data_use_recorders_.end());
      std::pair<int, int> frame_key =
          RenderFrameHostID(render_process_id, render_frame_id);
      entry->set_main_frame_id(frame_key);
      render_frame_data_use_map_.insert(std::make_pair(frame_key, entry));
    }
    return;
  }

  DataUseRecorderEntry entry = navigation_iter->second;
  pending_navigation_data_use_map_.erase(navigation_iter);
  entry->set_main_frame_id(
      RenderFrameHostID(render_process_id, render_frame_id));

  // If the frame has already been deleted then mark this navigation as having
  // completed its data use.
  if (frame_it == render_frame_data_use_map_.end()) {
    if (entry->IsDataUseComplete()) {
      OnDataUseCompleted(entry);
      data_use_recorders_.erase(entry);
    }
    return;
  }
  DataUseRecorderEntry old_frame_entry = frame_it->second;
  if (is_same_page_navigation) {
    old_frame_entry->MergeFrom(&(*entry));

    for (auto request : entry->pending_url_requests()) {
      request->RemoveUserData(DataUseRecorderEntryAsUserData::kUserDataKey);
      request->SetUserData(DataUseRecorderEntryAsUserData::kUserDataKey,
                           new DataUseRecorderEntryAsUserData(old_frame_entry));
      old_frame_entry->AddPendingURLRequest(request);
    }

    entry->RemoveAllPendingURLRequests();

    data_use_recorders_.erase(entry);
  } else {
    // Navigation is not same page, so remove old entry from
    // |render_frame_data_use_map_|, possibly marking it complete.
    render_frame_data_use_map_.erase(frame_it);
    if (old_frame_entry->IsDataUseComplete()) {
      OnDataUseCompleted(old_frame_entry);
      data_use_recorders_.erase(old_frame_entry);
    }

    DataUse& data_use = entry->data_use();

    DCHECK(!data_use.url().is_valid() || data_use.url() == gurl)
        << "is valid: " << data_use.url().is_valid()
        << "; data_use.url(): " << data_use.url().spec()
        << "; gurl: " << gurl.spec();
    if (!data_use.url().is_valid()) {
      data_use.set_url(gurl);
    }

    render_frame_data_use_map_.insert(std::make_pair(
        RenderFrameHostID(render_process_id, render_frame_id), entry));
  }
}

void ChromeDataUseAscriber::OnDataUseCompleted(DataUseRecorderEntry entry) {
  // TODO(ryansturm): Notify observers that data use is complete.
}

std::unique_ptr<URLRequestClassifier>
ChromeDataUseAscriber::CreateURLRequestClassifier() const {
  return base::MakeUnique<ContentURLRequestClassifier>();
}

ChromeDataUseAscriber::DataUseRecorderEntry
ChromeDataUseAscriber::CreateNewDataUseRecorder(net::URLRequest* request) {
  DataUseRecorderEntry entry = data_use_recorders_.emplace(
      data_use_recorders_.end());
  if (request) {
    entry->AddPendingURLRequest(request);
    request->SetUserData(DataUseRecorderEntryAsUserData::kUserDataKey,
                         new DataUseRecorderEntryAsUserData(entry));
  }
  return entry;
}

}  // namespace data_use_measurement
