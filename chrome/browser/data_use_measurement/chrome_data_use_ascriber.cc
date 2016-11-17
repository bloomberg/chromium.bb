// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"

#include "base/memory/ptr_util.h"
#include "components/data_use_measurement/content/content_url_request_classifier.h"
#include "components/data_use_measurement/core/data_use_recorder.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
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

DataUseRecorder* ChromeDataUseAscriber::GetDataUseRecorder(
    net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return nullptr;
}

void ChromeDataUseAscriber::OnBeforeUrlRequest(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

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

  DataUseRecorderEntry entry = data_use_recorders_.insert(
      data_use_recorders_.end(), base::MakeUnique<DataUseRecorder>());
  request->SetUserData(DataUseRecorderEntryAsUserData::kUserDataKey,
                       new DataUseRecorderEntryAsUserData(entry));
  pending_navigation_data_use_map_.insert(
      std::make_pair(request_info->GetGlobalRequestID(), entry));
}

void ChromeDataUseAscriber::OnUrlRequestDestroyed(net::URLRequest* request) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  content::ResourceType resource_type = request_info
                                            ? request_info->GetResourceType()
                                            : content::RESOURCE_TYPE_LAST_TYPE;
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    // If request was not successful, then ReadyToCommitNavigation will not be
    // called. So delete the pending navigation DataUseRecorderEntry here.
    if (!request->status().is_success()) {
      DeletePendingNavigationEntry(request_info->GetGlobalRequestID());
    }
  }
}

void ChromeDataUseAscriber::RenderFrameCreated(int render_process_id,
                                               int render_frame_id,
                                               int parent_render_process_id,
                                               int parent_render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(kundaji): Point child render frames to the same DataUseRecorder as
  // parent render frame.
  DataUseRecorderEntry entry = data_use_recorders_.insert(
      data_use_recorders_.end(), base::MakeUnique<DataUseRecorder>());
  render_frame_data_use_map_.insert(std::make_pair(
      RenderFrameHostID(render_process_id, render_frame_id), entry));
}

void ChromeDataUseAscriber::RenderFrameDeleted(int render_process_id,
                                               int render_frame_id,
                                               int parent_render_process_id,
                                               int parent_render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  RenderFrameHostID key(render_process_id, render_frame_id);
  auto frame_iter = render_frame_data_use_map_.find(key);
  DCHECK(frame_iter != render_frame_data_use_map_.end());
  DataUseRecorderEntry entry = frame_iter->second;
  render_frame_data_use_map_.erase(frame_iter);

  data_use_recorders_.erase(entry);
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

  // TODO(kundaji): Move the DataUseRecorderEntry from pending navigation map
  // to render frame map if |is_same_page_navigation| is true. Otherwise,
  // merge it with the DataUseRecorderEntry in the render frame map.
  DeletePendingNavigationEntry(global_request_id);
}

void ChromeDataUseAscriber::DidRedirectMainFrameNavigation(
    GURL gurl,
    int render_process_id,
    int render_frame_id,
    void* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeDataUseAscriber::DeletePendingNavigationEntry(
    content::GlobalRequestID global_request_id) {
  auto navigation_iter =
      pending_navigation_data_use_map_.find(global_request_id);
  // Pending navigation entry will not be found if finish navigation
  // raced the URLRequest.
  if (navigation_iter != pending_navigation_data_use_map_.end()) {
    auto entry = navigation_iter->second;
    pending_navigation_data_use_map_.erase(navigation_iter);
    data_use_recorders_.erase(entry);
  }
}

std::unique_ptr<URLRequestClassifier>
ChromeDataUseAscriber::CreateURLRequestClassifier() const {
  return base::MakeUnique<ContentURLRequestClassifier>();
}

}  // namespace data_use_measurement
