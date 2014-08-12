// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_RENDERER_STATE_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_RENDERER_STATE_H_

#include <map>
#include <string>
#include <utility>

#include "base/memory/singleton.h"

namespace extensions {

class WebViewGuest;

// This class keeps track of <webview> renderer state for use on the IO thread.
// All methods should be called on the IO thread.
class WebViewRendererState {
 public:
  struct WebViewInfo {
    int embedder_process_id;
    int instance_id;
    std::string partition_id;
    std::string embedder_extension_id;
  };

  static WebViewRendererState* GetInstance();

  // Looks up the information for the embedder <webview> for a given render
  // view, if one exists. Called on the IO thread.
  bool GetInfo(int guest_process_id, int guest_routing_id,
               WebViewInfo* webview_info);

  // Looks up the partition info for the embedder <webview> for a given guest
  // process. Called on the IO thread.
  bool GetPartitionID(int guest_process_id, std::string* partition_id);

  // Returns true if the given renderer is used by webviews.
  bool IsGuest(int render_process_id);

 private:
  friend class WebViewGuest;
  friend struct DefaultSingletonTraits<WebViewRendererState>;

  typedef std::pair<int, int> RenderId;
  typedef std::map<RenderId, WebViewInfo> WebViewInfoMap;

  struct WebViewPartitionInfo {
    int web_view_count;
    std::string partition_id;
    WebViewPartitionInfo() {}
    WebViewPartitionInfo(int count, std::string partition):
      web_view_count(count),
      partition_id(partition) {}
  };

  typedef std::map<int, WebViewPartitionInfo> WebViewPartitionIDMap;

  WebViewRendererState();
  ~WebViewRendererState();

  // Adds or removes a <webview> guest render process from the set.
  void AddGuest(int render_process_host_id, int routing_id,
                const WebViewInfo& webview_info);
  void RemoveGuest(int render_process_host_id, int routing_id);

  WebViewInfoMap webview_info_map_;
  WebViewPartitionIDMap webview_partition_id_map_;

  DISALLOW_COPY_AND_ASSIGN(WebViewRendererState);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_RENDERER_STATE_H_
