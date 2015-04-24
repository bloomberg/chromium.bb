// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_RENDERER_STATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_RENDERER_STATE_H_

#include <map>
#include <set>
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
    int rules_registry_id;
    std::string partition_id;
    std::string owner_host;
    std::set<int> content_script_ids;

    WebViewInfo();
    ~WebViewInfo();
  };

  static WebViewRendererState* GetInstance();

  // Looks up the information for the embedder <webview> for a given render
  // view, if one exists. Called on the IO thread.
  bool GetInfo(int guest_process_id, int guest_routing_id,
               WebViewInfo* web_view_info);

  // Looks up the information for the owner for a given guest process in a
  // <webview>. Called on the IO thread.
  bool GetOwnerInfo(int guest_process_id,
                    int* owner_process_id,
                    std::string* owner_host) const;

  // Looks up the partition info for the embedder <webview> for a given guest
  // process. Called on the IO thread.
  bool GetPartitionID(int guest_process_id, std::string* partition_id);

  // Returns true if the given renderer is used by webviews.
  bool IsGuest(int render_process_id);

  void AddContentScriptIDs(int embedder_process_id,
                           int view_instance_id,
                           const std::set<int>& script_ids);
  void RemoveContentScriptIDs(int embedder_process_id,
                              int view_instance_id,
                              const std::set<int>& script_ids);

 private:
  friend class WebViewGuest;
  friend struct DefaultSingletonTraits<WebViewRendererState>;

  using RenderId = std::pair<int, int>;
  using WebViewInfoMap = std::map<RenderId, WebViewInfo>;

  struct WebViewPartitionInfo {
    int web_view_count;
    std::string partition_id;
    WebViewPartitionInfo() {}
    WebViewPartitionInfo(int count, std::string partition):
      web_view_count(count),
      partition_id(partition) {}
  };

  using WebViewPartitionIDMap = std::map<int, WebViewPartitionInfo>;

  WebViewRendererState();
  ~WebViewRendererState();

  // Adds or removes a <webview> guest render process from the set.
  void AddGuest(int render_process_host_id, int routing_id,
                const WebViewInfo& web_view_info);
  void RemoveGuest(int render_process_host_id, int routing_id);

  WebViewInfoMap web_view_info_map_;
  WebViewPartitionIDMap web_view_partition_id_map_;

  DISALLOW_COPY_AND_ASSIGN(WebViewRendererState);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_RENDERER_STATE_H_
