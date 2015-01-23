// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"

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
  return web_view_partition_id_map_.find(render_process_id) !=
         web_view_partition_id_map_.end();
}

void WebViewRendererState::AddGuest(int guest_process_id,
                                    int guest_routing_id,
                                    const WebViewInfo& web_view_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RenderId render_id(guest_process_id, guest_routing_id);
  bool updating =
      web_view_info_map_.find(render_id) != web_view_info_map_.end();
  web_view_info_map_[render_id] = web_view_info;
  if (updating)
    return;

  auto iter = web_view_partition_id_map_.find(guest_process_id);
  if (iter != web_view_partition_id_map_.end()) {
    ++iter->second.web_view_count;
    return;
  }
  WebViewPartitionInfo partition_info(1, web_view_info.partition_id);
  web_view_partition_id_map_[guest_process_id] = partition_info;
}

void WebViewRendererState::RemoveGuest(int guest_process_id,
                                       int guest_routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RenderId render_id(guest_process_id, guest_routing_id);
  web_view_info_map_.erase(render_id);
  auto iter = web_view_partition_id_map_.find(guest_process_id);
  if (iter != web_view_partition_id_map_.end() &&
      iter->second.web_view_count > 1) {
    --iter->second.web_view_count;
    return;
  }
  web_view_partition_id_map_.erase(guest_process_id);
}

bool WebViewRendererState::GetInfo(int guest_process_id,
                                   int guest_routing_id,
                                   WebViewInfo* web_view_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RenderId render_id(guest_process_id, guest_routing_id);
  auto iter = web_view_info_map_.find(render_id);
  if (iter != web_view_info_map_.end()) {
    *web_view_info = iter->second;
    return true;
  }
  return false;
}

bool WebViewRendererState::GetOwnerInfo(int guest_process_id,
                                        int* owner_process_id,
                                        std::string* owner_extension_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(fsamuel): Store per-process info in WebViewPartitionInfo instead of in
  // WebViewInfo.
  for (const auto& info : web_view_info_map_) {
    if (info.first.first == guest_process_id) {
      if (owner_process_id)
        *owner_process_id = info.second.embedder_process_id;
      if (owner_extension_id)
        *owner_extension_id = info.second.owner_extension_id;
      return true;
    }
  }
  return false;
}

bool WebViewRendererState::GetPartitionID(int guest_process_id,
                                          std::string* partition_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = web_view_partition_id_map_.find(guest_process_id);
  if (iter != web_view_partition_id_map_.end()){
    *partition_id = iter->second.partition_id;
    return true;
  }
  return false;
}

}  // namespace extensions
