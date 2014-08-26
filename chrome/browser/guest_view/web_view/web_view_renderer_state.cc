// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/web_view_renderer_state.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

// static
WebViewRendererState* WebViewRendererState::GetInstance() {
  return Singleton<WebViewRendererState>::get();
}

WebViewRendererState::WebViewRendererState() {
}

WebViewRendererState::~WebViewRendererState() {
}

bool WebViewRendererState::IsGuest(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return webview_partition_id_map_.find(render_process_id) !=
         webview_partition_id_map_.end();
}

void WebViewRendererState::AddGuest(int guest_process_id,
                                    int guest_routing_id,
                                    const WebViewInfo& webview_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RenderId render_id(guest_process_id, guest_routing_id);
  webview_info_map_[render_id] = webview_info;
  WebViewPartitionIDMap::iterator iter =
      webview_partition_id_map_.find(guest_process_id);
  if (iter != webview_partition_id_map_.end()) {
    ++iter->second.web_view_count;
    return;
  }
  WebViewPartitionInfo partition_info(1, webview_info.partition_id);
  webview_partition_id_map_[guest_process_id] = partition_info;
}

void WebViewRendererState::RemoveGuest(int guest_process_id,
                                       int guest_routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RenderId render_id(guest_process_id, guest_routing_id);
  webview_info_map_.erase(render_id);
  WebViewPartitionIDMap::iterator iter =
      webview_partition_id_map_.find(guest_process_id);
  if (iter != webview_partition_id_map_.end() &&
      iter->second.web_view_count > 1) {
    --iter->second.web_view_count;
    return;
  }
  webview_partition_id_map_.erase(guest_process_id);
}

bool WebViewRendererState::GetInfo(int guest_process_id,
                                   int guest_routing_id,
                                   WebViewInfo* webview_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RenderId render_id(guest_process_id, guest_routing_id);
  WebViewInfoMap::iterator iter = webview_info_map_.find(render_id);
  if (iter != webview_info_map_.end()) {
    *webview_info = iter->second;
    return true;
  }
  return false;
}

bool WebViewRendererState::GetPartitionID(int guest_process_id,
                                          std::string* partition_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  WebViewPartitionIDMap::iterator iter =
      webview_partition_id_map_.find(guest_process_id);
  if (iter != webview_partition_id_map_.end()) {
    *partition_id = iter->second.partition_id;
    return true;
  }
  return false;
}

}  // namespace extensions
